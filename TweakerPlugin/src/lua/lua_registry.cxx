#include "pch.hxx"

#include "lua/lua_registry.hxx"

#include "lua/api/api_hooks.hxx"
#include "lua/lua_config.hxx"
#include "lua/lua_diag.hxx"
#include "lua/lua_sched.hxx"
#include "lua/lua_vm.hxx"

#include "plugin/diagnostics.hxx"
#include "plugin/paths.hxx"

namespace
{
std::vector<tw::lua::script> g_scripts;

std::string_view first_line(std::string_view text) noexcept
{
    const std::size_t end = text.find_first_of("\r\n");
    return end == std::string_view::npos ? text : text.substr(0, end);
}

// Runs one script's file from disk, from a clean slate. The body's failure is the script's own - a
// `failed` flag and a record in its diagnostics - never the layer's.
bool run(tw::lua::script& script) noexcept
{
    tw::lua::reset_health(script);
    tw::lua::diag::clear(script.id);

    std::string error;
    if(tw::lua::vm::run_file(script.path, script.id, error)) {
        script.failed = false;
        script.error.clear();
        return true;
    }

    script.failed = true;
    script.error.assign(first_line(error));
    tw::lua::diag::report(script.id, tw::lua::diag::level::error, "load", error);
    return false;
}
} // namespace

namespace tw::lua::registry
{
void load_all(const std::filesystem::path& directory) noexcept
{
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for(const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, ec)) {
        if(entry.is_regular_file(ec) && entry.path().extension() == L".lua") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);

    g_scripts.clear();
    g_scripts.reserve(files.size());

    for(const std::filesystem::path& path : files) {
        script entry;
        entry.id = static_cast<int>(g_scripts.size());
        entry.path = path;
        entry.file = tw::plugin::paths::to_utf8(path.filename());
        read_header(entry);
        entry.enabled = tw::lua::config::enabled(entry.file);
        g_scripts.push_back(std::move(entry));
    }

    for(script& entry : g_scripts) {
        if(!entry.enabled) {
            TW_LOG_INFO("lua: {} is disabled - not loading", entry.file);
            continue;
        }

        if(run(entry)) {
            TW_LOG_INFO("lua: loaded {}", entry.file);
            continue;
        }

        // Whatever the body registered before it threw goes too - the same rule the toggle follows.
        // A half-run script is not a script that runs half its features; it is a failed one, and it
        // stays switched on only so that the next toggle retries it.
        tw::lua::sched::unload(entry.id);
    }
}

void clear() noexcept
{
    g_scripts.clear();
}

int count() noexcept
{
    return static_cast<int>(g_scripts.size());
}

script* find(int id) noexcept
{
    if(id < 0 || id >= static_cast<int>(g_scripts.size())) {
        return nullptr;
    }

    return &g_scripts[static_cast<std::size_t>(id)];
}

int loaded_count() noexcept
{
    int loaded = 0;
    for(const script& entry : g_scripts) {
        if(entry.enabled && !entry.failed) {
            ++loaded;
        }
    }

    return loaded;
}

bool set_enabled(int id, bool enabled) noexcept
{
    script* const entry = find(id);
    if(entry == nullptr || tw::lua::vm::state() == nullptr) {
        return false;
    }

    if(entry->enabled == enabled && !entry->failed) {
        return entry->enabled;
    }

    // A dispatcher that failed on its own (a prelude bug, not a script's) stays off until something
    // changes. Changing the set of running scripts is exactly that moment - and if the bug is still
    // there, it switches straight back off on the next frame at no cost.
    tw::lua::sched::rearm();

    if(!enabled) {
        tw::lua::sched::unload(entry->id);
        entry->enabled = false;
        entry->failed = false;
        entry->error.clear();
        tw::lua::reset_health(*entry);
        tw::lua::diag::clear(entry->id);

        tw::lua::config::set_enabled(entry->file, false);
        tw::lua::config::save();

        TW_LOG_INFO("lua: disabled {}", entry->file);
        return false;
    }

    // Enabling is a fresh run from disk, not the resumption of anything - see the header. Unload
    // first anyway: a failed load can have registered handlers before it threw.
    tw::lua::sched::unload(entry->id);
    read_header(*entry);

    // Enabled before the body runs, not after: the body's own registrations are attributed to it,
    // and the scheduler only runs callbacks of scripts that are switched on.
    entry->enabled = true;

    if(!run(*entry)) {
        tw::lua::sched::unload(entry->id);
        entry->enabled = false;
        return false;
    }

    tw::lua::config::set_enabled(entry->file, true);
    tw::lua::config::save();

    TW_LOG_INFO("lua: enabled {}", entry->file);
    return true;
}

void reload(int id) noexcept
{
    const script* const entry = find(id);
    if(entry == nullptr || !entry->enabled) {
        return;
    }

    // Through the disable/enable path so there is exactly one teardown-and-run sequence to get right.
    (void)set_enabled(id, false);
    (void)set_enabled(id, true);
}
} // namespace tw::lua::registry
