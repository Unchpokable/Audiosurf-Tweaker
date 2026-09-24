#include "pch.hxx"

#include "lua/lua_script.hxx"

#include "plugin/paths.hxx"

namespace
{
std::string_view trim(std::string_view text) noexcept
{
    while(!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    while(!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }

    return text;
}
} // namespace

namespace tw::lua
{
const char* callback_name(callback kind) noexcept
{
    switch(kind) {
    case callback::frame:
        return "on_frame";
    case callback::tick:
        return "on_tick";
    case callback::post_tick:
        return "on_post_tick";
    case callback::call:
        return "on_call";
    case callback::ready:
        return "on_ready";
    case callback::state:
        return "on_state";
    case callback::group:
        return "on_group";
    case callback::unload:
        return "on_unload";
    }

    return "callback";
}

const char* state_name(script_state state) noexcept
{
    switch(state) {
    case script_state::off:
        return "off";
    case script_state::failed:
        return "failed";
    case script_state::suspended:
        return "suspended";
    case script_state::waiting:
        return "waiting";
    case script_state::running:
        return "running";
    }

    return "";
}

// Scanning stops at the first line that is neither blank nor a comment - the header is a header, and
// a stray `@author` in a comment three hundred lines down is not metadata.
void read_header(script& entry) noexcept
{
    entry.name = tw::plugin::paths::to_utf8(entry.path.stem());
    entry.author.clear();
    entry.version.clear();
    entry.description.clear();

    std::ifstream file { entry.path };
    if(!file.is_open()) {
        return;
    }

    std::string line;
    while(std::getline(file, line)) {
        std::string_view text = trim(line);

        if(text.empty()) {
            continue;
        }
        if(!text.starts_with("--")) {
            break;
        }

        text.remove_prefix(2);
        while(!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '-')) {
            text.remove_prefix(1);
        }

        if(text.empty() || text.front() != '@') {
            continue;
        }
        text.remove_prefix(1);

        const auto space = text.find_first_of(" \t");
        if(space == std::string_view::npos) {
            continue;
        }

        const std::string_view key = text.substr(0, space);
        std::string_view value = text.substr(space + 1);
        while(!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.remove_prefix(1);
        }

        if(value.empty()) {
            continue;
        }

        if(key == "name") {
            entry.name.assign(value);
        }
        else if(key == "author") {
            entry.author.assign(value);
        }
        else if(key == "version") {
            entry.version.assign(value);
        }
        else if(key == "description") {
            entry.description.assign(value);
        }
    }
}

void reset_health(script& entry) noexcept
{
    entry.health = script_health {};
}
} // namespace tw::lua
