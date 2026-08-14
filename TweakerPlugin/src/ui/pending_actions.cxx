// No TweakerPlugin PCH here - this TU is shared with smoke_test (see src/ui/CMakeLists.txt) and
// has no D3D9/Quest3D dependency, only STL - same convention as overlay_state.cxx/texture_cache.cxx.
#include "ui/pending_actions.hxx"

#include "ui/plugins/static/notefeed.hxx"
#include "ui/wire_text.hxx"

#include <array>
#include <chrono>
#include <string>

namespace
{
using tw::ui::overlay_state::tweak_id;
using my_clock_t = std::chrono::steady_clock;

// Matches OverlayHelper.cs's own HandshakeTimeout on the host - a reasonable "something's wrong,
// give up" window for a round trip that's otherwise just a few WM_COPYDATA hops.
constexpr auto k_confirm_timeout = std::chrono::milliseconds { 5000 };

// Bigger than tweak confirm timeout because it takes a some time to install skin
constexpr auto k_skin_confirm_timeout = std::chrono::milliseconds { 10000 };

struct pending_tweak {
    bool desired_enabled = false;
    my_clock_t::time_point deadline {};
    bool active = false;
};

struct pending_skin {
    std::string desired_name;
    my_clock_t::time_point deadline {};
    bool active = false;
};

tw::ui::pending_actions::send_fn g_send = nullptr;

std::array<pending_tweak, tw::ui::overlay_state::k_tweak_count> g_pending_tweaks {};
pending_skin g_pending_skin {};

// Mirrors overlay_state.cxx's own private tweak_index() - duplicated rather than exposed because
// it's an internal packing detail of that module, not part of its public contract. Both switches
// are driven by the same fixed 7-tweak enum and only ever change together.
std::size_t tweak_index(tweak_id id) noexcept
{
    switch(id) {
        case tweak_id::invisible_road:
            return 0;
        case tweak_id::hidden_song_title:
            return 1;
        case tweak_id::sidewinder_camera:
            return 2;
        case tweak_id::banking_camera:
            return 3;
        case tweak_id::freeride_no_blocks:
            return 4;
        case tweak_id::freeride_blocks_caterpillars:
            return 5;
        case tweak_id::freeride_auto_advance_disable:
            return 6;
        default:
            return 0;
    }
}

using tw::ui::wire::percent_encode;
} // namespace

namespace tw::ui::pending_actions
{
void set_send_backend(send_fn fn) noexcept
{
    g_send = fn;
}

void request_tweak(overlay_state::tweak_id id, bool desired_enabled)
{
    if(id == overlay_state::tweak_id::unknown) {
        return;
    }

    auto& slot = g_pending_tweaks[tweak_index(id)];
    slot.desired_enabled = desired_enabled;
    slot.deadline = my_clock_t::now() + k_confirm_timeout;
    slot.active = true;

    if(g_send != nullptr) {
        std::string op = "NOTIFY_TWEAK ";
        op += overlay_state::tweak_wire_name(id);
        op += desired_enabled ? " true" : " false";
        g_send(op);
    }
}

void request_skin(std::string_view desired_name)
{
    g_pending_skin.desired_name = desired_name;
    g_pending_skin.deadline = my_clock_t::now() + k_skin_confirm_timeout;
    g_pending_skin.active = true;

    if(g_send != nullptr) {
        std::string op = "NOTIFY_SKIN ";
        op += percent_encode(desired_name);
        g_send(op);
    }
}

void update(const overlay_state::cache& snapshot)
{
    const auto now = my_clock_t::now();

    for(std::size_t i = 0; i < g_pending_tweaks.size(); ++i) {
        pending_tweak& slot = g_pending_tweaks[i];
        if(!slot.active) {
            continue;
        }

        const auto id = overlay_state::all_tweak_ids()[i];
        if(overlay_state::is_tweak_enabled(snapshot, id) == slot.desired_enabled && !overlay_state::is_tweak_quick_player(snapshot, id)) {
            slot.active = false;
            continue;
        }

        if(now >= slot.deadline) {
            slot.active = false;
            std::string text = slot.desired_enabled ? "Failed to enable " : "Failed to disable ";
            text += overlay_state::tweak_display_name(id);
            tw::ui::plugins::statics::notefeed::push(text);
        }
    }

    if(g_pending_skin.active) {
        if(snapshot.current_skin_name == g_pending_skin.desired_name) {
            g_pending_skin.active = false;
        }
        else if(now >= g_pending_skin.deadline) {
            g_pending_skin.active = false;
            tw::ui::plugins::statics::notefeed::push("Failed to apply skin: " + g_pending_skin.desired_name);
        }
    }
}

bool tweak_display_enabled(const overlay_state::cache& snapshot, overlay_state::tweak_id id) noexcept
{
    if(id == overlay_state::tweak_id::unknown) {
        return false;
    }

    const pending_tweak& slot = g_pending_tweaks[tweak_index(id)];
    if(slot.active) {
        return slot.desired_enabled;
    }

    return overlay_state::is_tweak_enabled(snapshot, id);
}

std::string_view skin_display_name(const overlay_state::cache& snapshot) noexcept
{
    if(g_pending_skin.active) {
        return g_pending_skin.desired_name;
    }

    return snapshot.current_skin_name;
}

bool has_pending_skin() noexcept
{
    return g_pending_skin.active;
}

void reset() noexcept
{
    g_pending_tweaks = {};
    g_pending_skin = {};
}
} // namespace tw::ui::pending_actions
