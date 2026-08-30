#include "pch.hxx"

#include "lua/lua_config.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
std::string g_path;

// Only the scripts that differ from the default. See the header: absence means enabled.
std::vector<std::string> g_disabled;

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

bool listed(std::string_view file) noexcept
{
    return std::ranges::find(g_disabled, file) != g_disabled.end();
}
} // namespace

namespace tw::lua::config
{
void load(std::string_view path)
{
    g_path.assign(path);
    g_disabled.clear();

    std::ifstream file { g_path };
    if(!file.is_open()) {
        // No file is the normal first run, and unlike the skybox's config there is nothing useful to
        // write out: an empty disabled-set is exactly what "no file" already means.
        return;
    }

    std::string line;
    while(std::getline(file, line)) {
        const std::string_view trimmed = trim(line);
        if(trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        const auto eq = trimmed.find('=');
        if(eq == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim(trimmed.substr(0, eq));
        const std::string_view value = trim(trimmed.substr(eq + 1));

        if(!key.starts_with("script.")) {
            continue;
        }

        const std::string_view name = key.substr(std::string_view { "script." }.size());
        if(name.empty()) {
            continue;
        }

        if((value == "0" || value == "false" || value == "off") && !listed(name)) {
            g_disabled.emplace_back(name);
        }
    }

    TW_LOG_INFO("lua_config: loaded '{}' ({} script(s) disabled)", g_path, g_disabled.size());
}

void save()
{
    if(g_path.empty()) {
        return;
    }

    std::ofstream file { g_path, std::ios::trunc };
    if(!file.is_open()) {
        TW_LOG_WARNING("lua_config: cannot write '{}'", g_path);
        return;
    }

    file << "# Scripts the overlay should not run. Anything not listed here is enabled, so a new\n";
    file << "# .lua dropped into scripts/ starts working without editing this file.\n";
    for(const std::string& name : g_disabled) {
        file << "script." << name << "=0\n";
    }
}

bool enabled(std::string_view file) noexcept
{
    return !listed(file);
}

void set_enabled(std::string_view file, bool value)
{
    if(value) {
        std::erase(g_disabled, std::string { file });
    }
    else if(!listed(file)) {
        g_disabled.emplace_back(file);
    }
}
} // namespace tw::lua::config
