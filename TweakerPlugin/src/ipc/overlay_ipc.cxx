#include "pch.hxx"

#include "ipc/overlay_ipc.hxx"

#include "framework/wndproc_hub.hxx"

#include "plugin/diagnostics.hxx"

#include "ui/overlay_state.hxx"
#include "ui/pending_actions.hxx"
#include "ui/wire_text.hxx"

#include "ui/qp/qp_pending.hxx"
#include "ui/qp/qp_state.hxx"
#include "ui/qp/qp_wire.hxx"

namespace
{
constexpr std::string_view k_tw_ovl_prefix = "TW_OVL ";

// Sized for the largest *fixed-arity* argument list - TWEAK_SET <name> <bool> <source>. SKIN_LIST is
// variable-arity (one token per catalog entry) and is parsed separately via for_each_token(),
// never through this fixed-size splitter, so it isn't bounded by this constant.
constexpr std::size_t k_max_fixed_op_tokens = 3;

// Both only ever written from the IPC thread today, but reads may come from the UI/render thread
// once future outbound ops (NOTIFY_TWEAK etc., see plugin-state-ops) can be triggered from there -
// atomic keeps that cross-thread read well-defined without needing a full mutex for two small
// values.
std::atomic<HWND> g_bridge_hwnd { nullptr };
std::atomic<bool> g_handshake_complete { false };

using tw::ui::wire::percent_decode;

// Splits off the first whitespace-delimited token (the OP) and returns the remainder unparsed, so
// each op handler below can decide for itself whether to further split via the bounded
// split_fixed_tokens() (small, fixed-arity ops) or the unbounded for_each_token() (SKIN_LIST).
std::pair<std::string_view, std::string_view> split_first_token(std::string_view s) noexcept
{
    while(!s.empty() && s.front() == ' ') {
        s.remove_prefix(1);
    }

    const auto space = s.find(' ');
    if(space == std::string_view::npos) {
        return { s, std::string_view {} };
    }

    return { s.substr(0, space), s.substr(space + 1) };
}

// Bounded splitter for small, fixed-arity op arguments (HANDSHAKE_BEGIN, TWEAK_SET, CURRENT_SKIN). Never
// used for SKIN_LIST - see for_each_token() below.
std::array<std::string_view, k_max_fixed_op_tokens> split_fixed_tokens(std::string_view s, std::size_t& out_count) noexcept
{
    std::array<std::string_view, k_max_fixed_op_tokens> tokens {};
    out_count = 0;

    while(!s.empty() && s.front() == ' ') {
        s.remove_prefix(1);
    }

    while(!s.empty() && out_count < tokens.size()) {
        const auto space = s.find(' ');
        if(space == std::string_view::npos) {
            tokens[out_count++] = s;
            break;
        }

        tokens[out_count++] = s.substr(0, space);
        s.remove_prefix(space + 1);
        while(!s.empty() && s.front() == ' ') {
            s.remove_prefix(1);
        }
    }

    return tokens;
}

// Unbounded token iteration for ops whose arity depends on host-side data (currently only
// SKIN_LIST, one token per catalog entry). No fixed cap, no intermediate array - callers push
// straight into their own container (e.g. a std::vector already being reserved for the result).
template<typename F>
void for_each_token(std::string_view s, F&& fn)
{
    while(!s.empty() && s.front() == ' ') {
        s.remove_prefix(1);
    }

    while(!s.empty()) {
        const auto space = s.find(' ');
        if(space == std::string_view::npos) {
            std::invoke(std::forward<F>(fn), s);
            return;
        }

        std::invoke(std::forward<F>(fn), s.substr(0, space));
        s.remove_prefix(space + 1);
        while(!s.empty() && s.front() == ' ') {
            s.remove_prefix(1);
        }
    }
}

// Mirrors ASBridge/src/service/service.cxx's send_ansi_copydata: WM_COPYDATA's COPYDATASTRUCT is
// passed through as raw bytes by the OS's ANSI/Unicode message-thunking layer regardless of
// whether SendMessageA or SendMessageW is used to deliver it - unlike text-carrying messages
// (WM_SETTEXT etc.), lpData/cbData are never reinterpreted or retranscoded. SendMessageW here is
// therefore safe with a raw (narrow) char* payload, matching the pattern already established and
// in production use on the ASBridge side.
bool send_payload_to_bridge(std::string_view inner_payload)
{
    const HWND target = g_bridge_hwnd.load(std::memory_order_relaxed);
    if(target == nullptr) {
        // Expected on every build where asbridge still doesn't send HANDSHAKE_BEGIN (see
        // Docs/Internal/overlay-protocol.md, "Чего пока нет") - which means every outgoing
        // NOTIFY_TWEAK/NOTIFY_SKIN is dropped here and the overlay's optimistic toggle will roll
        // back with a "Failed to ..." toast ~5s later. Logged so that symptom is traceable to its
        // actual cause instead of looking like a plugin bug.
        TW_LOG_WARNING("ipc: dropping outbound '{}' - no bridge window (HANDSHAKE_BEGIN never arrived)", inner_payload);
        return false;
    }

    std::string envelope;
    envelope.reserve(k_tw_ovl_prefix.size() + inner_payload.size());
    envelope.append(k_tw_ovl_prefix.begin(), k_tw_ovl_prefix.end());
    envelope.append(inner_payload.begin(), inner_payload.end());

    COPYDATASTRUCT cds {};
    cds.dwData = 0;
    cds.cbData = static_cast<DWORD>(envelope.size() + 1);
    cds.lpData = const_cast<char*>(envelope.c_str());

    return ::SendMessageW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds)) != 0;
}

// Parses and applies a single TW_OVL op synchronously, right where it's received - see
// handle_copydata() below for why there's no inbound queue anymore.
void handle_tw_ovl_op(std::string_view payload)
{
    const auto [op, rest] = split_first_token(payload);
    if(op.empty()) {
        return;
    }

    TW_LOG_DEBUG("ipc: <- '{}'", payload);

    // Quick Player owns a whole family of ops (Docs/Internal/overlay-quickplayer.md) and is handed
    // the payload whole rather than parsed here: this module is the transport, and a dozen more
    // op names in this chain is exactly the "unreadable dumping ground" the QP module exists to
    // avoid.
    if(op.starts_with("QP_")) {
        tw::ui::qp::handle_op(op, rest);
        return;
    }

    if(op == "HANDSHAKE_BEGIN") {
        std::size_t token_count = 0;
        const auto tokens = split_fixed_tokens(rest, token_count);
        if(token_count < 1) {
            return;
        }

        const std::string caption { tokens[0] };

        const HWND bridge = ::FindWindowA(nullptr, caption.c_str());
        g_bridge_hwnd.store(bridge, std::memory_order_relaxed);
        g_handshake_complete.store(true, std::memory_order_relaxed);

        if(bridge == nullptr) {
            // Most likely cause is a caption containing a space: L3 tokens are split on spaces and
            // the caption, unlike skin names, is not percent-encoded, so tokens[0] holds only the
            // first word. See Docs/Internal/overlay-protocol.md, HANDSHAKE_BEGIN.
            TW_LOG_ERROR("ipc: HANDSHAKE_BEGIN caption '{}' resolved to no window - outbound channel stays closed", caption);
        }
        else {
            TW_LOG_INFO("ipc: handshake complete, bridge window={} (caption '{}')", static_cast<const void*>(bridge), caption);
        }

        send_payload_to_bridge("HANDSHAKE_ACK");
        return;
    }

    if(op == "HANDSHAKE_PING") {
        // v1: no-op; reserved for future keepalive / reconnect.
        return;
    }

    if(op == "SKIN_LIST") {
        std::vector<std::string> names;
        for_each_token(rest, [&names](std::string_view token) {
            names.push_back(percent_decode(token));
        });
        tw::ui::overlay_state::set_skin_list(std::move(names));
        return;
    }

    if(op == "TWEAK_SET") {
        std::size_t token_count = 0;
        const auto tokens = split_fixed_tokens(rest, token_count);
        if(token_count < 2) {
            return;
        }

        const auto id = tw::ui::overlay_state::resolve_tweak_id(tokens[0]);
        if(id == tw::ui::overlay_state::tweak_id::unknown) {
            return;
        }

        const bool enabled = tokens[1] == "true" || tokens[1] == "1";
        const bool quick_player = token_count >= 3 && tokens[2] == "quick_player";
        tw::ui::overlay_state::set_tweak(id, enabled, quick_player);
        return;
    }

    if(op == "CURRENT_SKIN") {
        std::size_t token_count = 0;
        const auto tokens = split_fixed_tokens(rest, token_count);
        if(token_count < 1) {
            return;
        }

        tw::ui::overlay_state::set_current_skin(percent_decode(tokens[0]));
        return;
    }
}

bool handle_copydata(HWND /*hwnd*/, UINT /*msg*/, WPARAM /*wparam*/, LPARAM lparam, LRESULT& out_result)
{
    const auto* copydata = reinterpret_cast<COPYDATASTRUCT*>(lparam);
    if(copydata == nullptr || copydata->lpData == nullptr || copydata->cbData == 0) {
        return false;
    }

    const auto* raw = static_cast<const char*>(copydata->lpData);
    const auto raw_len = static_cast<std::size_t>(copydata->cbData);
    const auto data_len = raw_len > 0 ? raw_len - 1 : 0; // cbData includes null terminator

    std::string data;
    data.assign(raw, data_len);

    while(!data.empty() && data.back() == '\0') {
        data.pop_back();
    }

    TW_LOG_DEBUG("Received WM_COPYDATA: {}", data);

    const bool is_tw_ovl =
        data.size() >= k_tw_ovl_prefix.size() && std::equal(k_tw_ovl_prefix.begin(), k_tw_ovl_prefix.end(), data.begin());

    if(!is_tw_ovl) {
        return false;
    }

    // Parsed synchronously, right here on whichever thread delivers WM_COPYDATA - no inbound
    // queue: the wire payloads involved are tiny (a handful of tokens, at most a few dozen skin
    // names) and arrive rarely (handshake once, tweak toggles are user-driven, skin list on
    // install/connect), so this is not hot-path work by this project's own definition (EndScene/
    // CallChannel/per-frame code) even though it may now run off the render thread's timeline.
    handle_tw_ovl_op(std::string_view(data).substr(k_tw_ovl_prefix.size()));

    // v1 requirement: swallow TW_OVL messages and do not forward them into the game's own handler.
    out_result = TRUE;
    return true;
}
} // namespace

namespace tw::ipc
{
void initialize() noexcept
{
    tw::framework::wndproc::subscribe(WM_COPYDATA, &handle_copydata);
}

void shutdown() noexcept
{
    g_bridge_hwnd.store(nullptr, std::memory_order_relaxed);
    g_handshake_complete.store(false, std::memory_order_relaxed);
    tw::ui::overlay_state::reset();
    tw::ui::qp::state::reset();
    tw::ui::pending_actions::reset();
    tw::ui::qp::pending::reset();
}

bool send_overlay_command(std::string_view op_line)
{
    return send_payload_to_bridge(op_line);
}
} // namespace tw::ipc
