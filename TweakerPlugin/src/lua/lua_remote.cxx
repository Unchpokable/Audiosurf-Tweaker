#include "pch.hxx"

#include "lua/lua_remote.hxx"

#include "lua/lua_registry.hxx"
#include "lua/lua_vm.hxx"

#include "plugin/diagnostics.hxx"

#include "ui/wire_text.hxx"

namespace
{
// A message is cut to this many bytes before it is encoded (overlay-scripts.md). Not for the transport -
// the pipe takes any length - but because the full text and the traceback are the overlay tab's, and a
// row in TweakerUI wants one line.
constexpr std::size_t k_message_limit = 400;

enum class command_kind : std::uint8_t {
    set,
    reload,
    rescan,
    sync,
    order,
};

struct command {
    command_kind kind = command_kind::sync;
    std::string file;
    bool enabled = false;
    // SCRIPT_ORDER's file names, top first.
    std::vector<std::string> files;
};

// Written by the IPC thread, emptied by the engine thread. `g_mail` is what keeps the empty case to one
// load: the mutex is only taken when there is something to take.
std::mutex g_mailbox_mutex;
std::vector<command> g_mailbox;
std::atomic<bool> g_mail { false };

tw::lua::remote::send_fn g_send = nullptr;
tw::lua::remote::present_fn g_present = nullptr;

// What the host was last told, per script id; empty where nothing was sent yet. Engine thread only.
std::vector<std::optional<std::string>> g_sent;
std::uint32_t g_seen_revision = 0;

// The SCRIPT_ORDER line the host was last sent; empty when none was. Engine thread only.
std::string g_sent_order;

void post(command entry)
{
    {
        std::lock_guard lock(g_mailbox_mutex);
        g_mailbox.push_back(std::move(entry));
    }
    g_mail.store(true, std::memory_order_release);
}

std::pair<std::string_view, std::string_view> split_token(std::string_view text) noexcept
{
    while(!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }

    const std::size_t space = text.find(' ');
    if(space == std::string_view::npos) {
        return { text, {} };
    }

    return { text.substr(0, space), text.substr(space + 1) };
}

// First line, at most k_message_limit bytes, and never half a UTF-8 sequence: the host decodes it as
// UTF-8, and a cut continuation byte would turn into a replacement character at the end.
std::string_view clip(std::string_view text) noexcept
{
    const std::size_t end = text.find_first_of("\r\n");
    if(end != std::string_view::npos) {
        text = text.substr(0, end);
    }

    if(text.size() <= k_message_limit) {
        return text;
    }

    std::size_t cut = k_message_limit;
    while(cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) {
        --cut;
    }

    return text.substr(0, cut);
}

bool connected() noexcept
{
    return g_send != nullptr && g_present != nullptr && g_present();
}

void send_state(std::string_view file, std::string_view description)
{
    (void)g_send(std::format("SCRIPT_STATE {} {}", tw::ui::wire::percent_encode(file), description));
}

// The record behind a file name, rescanning first when there is none or it has no file: a command about a
// file the plugin has not seen is a command about a file that was just installed (overlay-scripts.md).
// One rescan per batch is enough - a second would find the same directory.
tw::lua::script* resolve(std::string_view file, bool& rescanned) noexcept
{
    tw::lua::script* entry = tw::lua::registry::find(file);
    if((entry == nullptr || entry->missing) && !rescanned) {
        tw::lua::registry::rescan();
        rescanned = true;
        entry = tw::lua::registry::find(file);
    }

    return entry;
}

// The run order as one line: every record, missing ones included, top first (script-coexistence.md
// §3). The whole order rather than what moved: it is one short line, sent only when the order changed, and
// the host never has to rebuild it from pieces.
std::string order_line()
{
    std::string line = "SCRIPT_ORDER";
    for(const int id : tw::lua::registry::order()) {
        if(const tw::lua::script* const entry = tw::lua::registry::find(id); entry != nullptr) {
            line += ' ';
            line += tw::ui::wire::percent_encode(entry->file);
        }
    }

    return line;
}

// Tells the host what changed since it was last told - or, with `full`, everything, closed by
// SCRIPT_SYNCED. `forced` are ids whose state goes out even if it did not change: a SCRIPT_SET or
// SCRIPT_RELOAD is answered always, because the answer is also the confirmation. `unknown` are files a
// command named that are not there even after a rescan. `order_asked` is the same for SCRIPT_ORDER: the
// order goes out whether or not the host's request changed it.
void flush(bool full, std::span<const int> forced, std::span<const std::string> unknown, bool order_asked = false)
{
    g_seen_revision = tw::lua::registry::revision();

    if(!connected()) {
        g_sent.clear();
        g_sent_order.clear();
        return;
    }

    const int count = tw::lua::registry::count();
    g_sent.resize(static_cast<std::size_t>(count));

    for(int id = 0; id < count; ++id) {
        const tw::lua::script* const entry = tw::lua::registry::find(id);
        if(entry == nullptr) {
            continue;
        }

        std::string description = tw::lua::remote::describe(*entry);
        std::optional<std::string>& sent = g_sent[static_cast<std::size_t>(id)];

        const bool due = full || !sent.has_value() || *sent != description || std::ranges::find(forced, id) != forced.end();
        if(!due) {
            continue;
        }

        send_state(entry->file, description);
        sent = std::move(description);
    }

    for(const std::string& file : unknown) {
        send_state(file, "false missing");
    }

    // After the states, so a host that lays the order over its rows already has a row for every name in it.
    if(std::string order = order_line(); full || order_asked || order != g_sent_order) {
        (void)g_send(order);
        g_sent_order = std::move(order);
    }

    if(full) {
        (void)g_send(std::format("SCRIPT_SYNCED {}", count));
    }
}

void carry_out(std::span<const command> commands)
{
    bool full = false;
    bool rescanned = false;
    bool order_asked = false;
    std::vector<int> forced;
    std::vector<std::string> unknown;

    for(const command& entry : commands) {
        switch(entry.kind) {
        case command_kind::set:
        case command_kind::reload: {
            tw::lua::script* const script = resolve(entry.file, rescanned);
            if(script == nullptr) {
                TW_LOG_INFO("lua_remote: {} for {} - no such script, even after a rescan",
                    entry.kind == command_kind::set ? "SCRIPT_SET" : "SCRIPT_RELOAD",
                    entry.file);
                unknown.push_back(entry.file);
                break;
            }

            if(entry.kind == command_kind::set) {
                (void)tw::lua::registry::set_enabled(script->id, entry.enabled);
            }
            else {
                tw::lua::registry::reload(script->id);
            }
            forced.push_back(script->id);
            break;
        }

        case command_kind::rescan:
            if(!rescanned) {
                tw::lua::registry::rescan();
                rescanned = true;
            }
            full = true;
            break;

        case command_kind::sync:
            full = true;
            break;

        // Names the plugin does not know are skipped by set_order, not looked for: the order is of the
        // scripts the plugin has, and a file it has not seen yet goes last when a rescan finds it.
        case command_kind::order:
            tw::lua::registry::set_order(entry.files);
            order_asked = true;
            break;
        }
    }

    flush(full, forced, unknown, order_asked);
}
} // namespace

namespace tw::lua::remote
{
void set_link(send_fn send, present_fn present) noexcept
{
    g_send = send;
    g_present = present;
}

void handle_op(std::string_view op, std::string_view rest)
{
    const auto [first, tail] = split_token(rest);

    if(op == "SCRIPT_SET") {
        const std::string_view flag = split_token(tail).first;
        if(first.empty() || (flag != "true" && flag != "false")) {
            return;
        }
        post(command { command_kind::set, tw::ui::wire::percent_decode(first), flag == "true" });
        return;
    }

    if(op == "SCRIPT_RELOAD") {
        if(first.empty()) {
            return;
        }
        post(command { command_kind::reload, tw::ui::wire::percent_decode(first), false });
        return;
    }

    if(op == "SCRIPT_RESCAN") {
        post(command { command_kind::rescan, {}, false });
        return;
    }

    if(op == "SCRIPT_ORDER") {
        command entry { command_kind::order, {}, false };
        for(std::string_view tail_files = rest; !tail_files.empty();) {
            const auto [name, after] = split_token(tail_files);
            if(!name.empty()) {
                entry.files.push_back(tw::ui::wire::percent_decode(name));
            }
            tail_files = after;
        }
        post(std::move(entry));
        return;
    }
}

void request_sync()
{
    post(command { command_kind::sync, {}, false });
}

void pump() noexcept
{
    if(g_mail.load(std::memory_order_acquire)) [[unlikely]] {
        std::vector<command> commands;
        {
            std::lock_guard lock(g_mailbox_mutex);
            commands.swap(g_mailbox);
            g_mail.store(false, std::memory_order_relaxed);
        }

        if(tw::lua::vm::state() == nullptr) {
            return;
        }

        carry_out(commands);
        return;
    }

    if(tw::lua::registry::revision() != g_seen_revision) [[unlikely]] {
        flush(false, {}, {});
    }
}

std::string describe(const script& entry)
{
    // Precedence matches the overlay row's (lua_ui.cxx, state_of), minus `waiting`, which is per-frame and
    // stays in the overlay. A failed enable leaves `enabled` false, so `failed` can come with either.
    if(entry.missing) {
        return "false missing";
    }

    std::string_view status = "running";
    std::string_view message;

    if(entry.failed) {
        status = "failed";
        message = clip(entry.error);
    }
    else if(!entry.enabled) {
        status = "off";
    }
    else if(entry.health.suspended) {
        status = "suspended";
        message = clip(entry.health.reason);
    }

    std::string text = std::format("{} {}", entry.enabled ? "true" : "false", status);
    if(!message.empty()) {
        text += ' ';
        text += tw::ui::wire::percent_encode(message);
    }

    return text;
}
} // namespace tw::lua::remote
