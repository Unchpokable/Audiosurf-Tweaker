#include "pch.hxx"

#include "skybox/sky_remote.hxx"

#include "skybox/skybox.hxx"
#include "skybox/skybox_config.hxx"

#include "ui/wire_text.hxx"

namespace
{
enum class command_kind {
    enable,
    select,
    rescan,
    sync,
};

struct command {
    command_kind op {};

    bool enabled {};

    tw::skybox::entry_kind kind {};
    std::string id;
};

std::mutex g_mutex;
std::vector<command> g_commands;
std::atomic<bool> g_mail { false };

tw::skybox::remote::send_fn g_send = nullptr;
tw::skybox::remote::present_fn g_present = nullptr;

unsigned int g_revision = 0;
unsigned int g_catalog = 0;

std::string g_sent;
bool g_synced = false;

bool connected() noexcept
{
    return g_send != nullptr && g_present != nullptr && g_present();
}

void post(command value)
{
    const std::lock_guard lock { g_mutex };
    g_commands.push_back(std::move(value));
    g_mail.store(true, std::memory_order_release);
}

std::string_view token(std::string_view& rest) noexcept
{
    const auto start = rest.find_first_not_of(' ');

    if(start == std::string_view::npos) {
        rest = {};
        return {};
    }

    rest.remove_prefix(start);

    const auto end = rest.find(' ');
    const auto out = rest.substr(0, end);

    rest = end == std::string_view::npos ? std::string_view {} : rest.substr(end + 1);

    return out;
}

void state(bool force)
{
    if(!connected()) {
        g_sent.clear();
        g_synced = false;
        return;
    }

    std::string line = tw::skybox::remote::describe();

    if(force || line != g_sent) {
        if(g_send("SKY_STATE " + line)) {
            g_sent = std::move(line);
        }
    }

    g_revision = tw::skybox::revision();
}

void full()
{
    if(!connected()) {
        return;
    }

    const auto entries = tw::skybox::catalog();
    bool sent = true;

    for(const auto& entry : entries) {
        std::string line = std::format("SKY_ENTRY {} {} {}",
            tw::skybox::config::kind_key(entry.kind),
            tw::ui::wire::percent_encode(entry.id),
            tw::ui::wire::percent_encode(entry.display_name));

        if(entry.face_size > 0) {
            line += ' ';
            line += std::to_string(entry.face_size);
        }

        sent = g_send(line) && sent;
    }

    if(sent) {
        sent = g_send(std::format("SKY_SYNCED {}", entries.size()));
    }

    g_synced = sent;
    g_catalog = tw::skybox::catalog_generation();

    state(true);
}
} // namespace

namespace tw::skybox::remote
{
void set_link(send_fn send, present_fn present) noexcept
{
    g_send = send;
    g_present = present;
    g_synced = false;
    g_sent.clear();
}

void handle_op(std::string_view op, std::string_view rest)
{
    const auto first = token(rest);

    if(op == "SKY_ENABLE") {
        if((first == "true" || first == "false") && token(rest).empty()) {
            post({ command_kind::enable, first == "true", {}, {} });
        }
    }
    else if(op == "SKY_SELECT") {
        const auto kind = config::parse_kind(first);
        const auto id = token(rest);

        if(kind.has_value() && !id.empty() && token(rest).empty()) {
            auto decoded = tw::ui::wire::percent_decode(id);

            if(decoded.find('\0') == std::string::npos) {
                post({ command_kind::select, false, *kind, std::move(decoded) });
            }
        }
    }
    else if(op == "SKY_RESCAN" && first.empty()) {
        post({ command_kind::rescan, false, {}, {} });
    }
}

void request_sync()
{
    post({ command_kind::sync, false, {}, {} });
}

void pump() noexcept
{
    std::vector<command> commands;

    if(g_mail.load(std::memory_order_acquire)) {
        const std::lock_guard lock { g_mutex };
        commands.swap(g_commands);
        g_mail.store(false, std::memory_order_relaxed);
    }

    if(!connected()) {
        g_synced = false;
        g_sent.clear();
        return;
    }

    bool rescanned = false;
    bool send_full = !g_synced;

    const auto rescan = [&rescanned] {
        if(!rescanned) {
            refresh_catalog();
            rescanned = true;
        }
    };

    if(!catalog_ready()) {
        rescan();
    }

    for(const auto& item : commands) {
        switch(item.op) {
            case command_kind::enable:
                set_enabled(item.enabled);
                state(true);
                break;

            case command_kind::select:
                if(item.kind != entry_kind::program && item.kind != entry_kind::packed && catalog_index(item.kind, item.id) < 0) {
                    rescan();
                }

                select(item.kind, item.id);
                state(true);
                break;

            case command_kind::rescan:
                rescan();
                send_full = true;
                break;

            case command_kind::sync:
                rescan();
                send_full = true;
                break;
        }
    }

    if(send_full || g_catalog != catalog_generation()) {
        full();
    }
    else if(g_revision != revision()) {
        state(false);
    }
}

std::string describe()
{
    const auto current = current_load_status();
    const auto& selected = config::sky();

    std::string_view name;

    switch(current.state) {
        case load_state::off:
            name = "off";
            break;

        case load_state::missing:
            name = "missing";
            break;

        case load_state::compiling:
            name = "compiling";
            break;

        case load_state::failed:
            name = "failed";
            break;

        case load_state::waiting:
            name = "waiting";
            break;

        case load_state::ready:
            name = "ready";
            break;
    }

    std::string line = std::format(
        "{} {} {} {}", is_enabled() ? "true" : "false", config::kind_key(selected.kind), tw::ui::wire::percent_encode(selected.id), name);

    if(current.state == load_state::failed && !current.message.empty()) {
        line += ' ';
        line += tw::ui::wire::percent_encode(tw::ui::wire::first_line(current.message));
    }

    return line;
}
} // namespace tw::skybox::remote
