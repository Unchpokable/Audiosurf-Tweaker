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
// Owning pointers, not the scripts themselves: rescan() appends, and an append that reallocates would
// move every script out from under whoever holds a script* (see the header). The pointers move; the
// scripts do not. Reading through one is the same load as reading through a raw pointer.
std::vector<std::unique_ptr<tw::lua::script>> g_scripts;

// What load_all() was given, for rescan().
std::filesystem::path g_directory;

std::uint32_t g_revision = 0;

std::string_view first_line(std::string_view text) noexcept
{
    const std::size_t end = text.find_first_of("\r\n");
    return end == std::string_view::npos ? text : text.substr(0, end);
}

// Every .lua directly in `directory`, sorted by name - the order ids are handed out in.
std::vector<std::filesystem::path> list_files(const std::filesystem::path& directory) noexcept
{
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for(const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, ec)) {
        if(entry.is_regular_file(ec) && entry.path().extension() == L".lua") {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);

    return files;
}

// A record for one file: its header, and whether scripts.cfg has it on. Nothing is run.
std::unique_ptr<tw::lua::script> catalogue(const std::filesystem::path& path, int id) noexcept
{
    auto entry = std::make_unique<tw::lua::script>();
    entry->id = id;
    entry->path = path;
    entry->file = tw::plugin::paths::to_utf8(path.filename());
    tw::lua::read_header(*entry);
    entry->enabled = tw::lua::config::enabled(entry->file);

    return entry;
}

// The .map beside a bundled script (lua_linemap.hxx), read with the script rather than with its header:
// a rebuild replaces both, and the next run is when the new pair is looked at. Neither outcome here
// stops the script loading - a missing or unusable map only means bundle positions in its messages.
void load_map(tw::lua::script& script) noexcept
{
    std::string detail;
    switch(tw::lua::load_line_map(script.path, script.map, detail)) {
    case tw::lua::line_map_status::absent:
    case tw::lua::line_map_status::loaded:
        return;
    case tw::lua::line_map_status::malformed:
        tw::lua::diag::report(script.id, tw::lua::diag::level::warn, "map",
            std::format("{}.map is unusable ({}) - positions are shown as lines of the bundle", script.file, detail));
        return;
    case tw::lua::line_map_status::stale:
        tw::lua::diag::report(script.id, tw::lua::diag::level::info, "map",
            std::format("{}.map is ignored: {} - positions are shown as lines of this file", script.file, detail));
        return;
    }
}

// Runs one script's file from disk, from a clean slate. The body's failure is the script's own - a
// `failed` flag and a record in its diagnostics - never the layer's.
bool run(tw::lua::script& script) noexcept
{
    tw::lua::reset_health(script);
    tw::lua::diag::clear(script.id);
    load_map(script);

    // Built against a newer API than this plugin has: its first use of a name added since would fail
    // with a message about a nil, which says nothing about the actual problem. Saying which version it
    // needs is what the header's `@api` line is for (lua-engine-fix-roadmap.md §9.5).
    if(script.api > tw::lua::vm::api_version()) {
        script.failed = true;
        script.error = std::format(
            "needs scripting API {} - this Tweaker has API {}; update the Tweaker to run it", script.api, tw::lua::vm::api_version());
        tw::lua::diag::report(script.id, tw::lua::diag::level::error, "load", script.error);
        return false;
    }

    std::string error;
    if(tw::lua::vm::run_file(script.path, script.id, error)) {
        script.failed = false;
        script.error.clear();
        return true;
    }

    script.failed = true;
    std::string mapped;
    script.error.assign(first_line(tw::lua::map_positions(script.map, script.file, error, mapped) ? mapped : error));
    tw::lua::diag::report(script.id, tw::lua::diag::level::error, "load", error);
    return false;
}

// run(), plus the rule for a body that throws: whatever it registered before it threw goes too. A
// half-run script is not a script that runs half its features; it is a failed one. `enabled` is left
// as it was - the caller decides whether a failure switches the script off.
bool launch(tw::lua::script& script) noexcept
{
    if(run(script)) {
        return true;
    }

    tw::lua::sched::unload(script.id);
    return false;
}

// Everything a running script has, gone: its callbacks and hooks, its failure, its health, its
// messages. `enabled` and scripts.cfg are the caller's.
void stop(tw::lua::script& script) noexcept
{
    tw::lua::sched::unload(script.id);
    script.failed = false;
    script.error.clear();
    tw::lua::reset_health(script);
    tw::lua::diag::clear(script.id);
}
} // namespace

namespace tw::lua::registry
{
void load_all(const std::filesystem::path& directory) noexcept
{
    const std::vector<std::filesystem::path> files = list_files(directory);

    g_directory = directory;
    g_scripts.clear();
    g_scripts.reserve(files.size());

    for(const std::filesystem::path& path : files) {
        g_scripts.push_back(catalogue(path, static_cast<int>(g_scripts.size())));
    }

    for(const std::unique_ptr<script>& entry : g_scripts) {
        if(!entry->enabled) {
            TW_LOG_INFO("lua: {} is disabled - not loading", entry->file);
            continue;
        }

        // A script that fails here stays switched on, so that the next toggle or reload retries it.
        if(launch(*entry)) {
            TW_LOG_INFO("lua: loaded {}", entry->file);
        }
    }

    changed();
}

void rescan() noexcept
{
    if(g_directory.empty() || tw::lua::vm::state() == nullptr) {
        return;
    }

    const std::vector<std::filesystem::path> files = list_files(g_directory);

    std::vector<std::string> names;
    names.reserve(files.size());
    for(const std::filesystem::path& path : files) {
        names.push_back(tw::plugin::paths::to_utf8(path.filename()));
    }

    // A dispatcher that failed on its own stays off until the set of running scripts changes, and this
    // may well change it - see set_enabled().
    tw::lua::sched::rearm();

    int gone = 0;
    int back = 0;
    int added = 0;

    // Known files first: the ones that went away, the ones that came back, and fresh headers for the
    // rest. What is running keeps running - a rescan picks up files, it does not restart anything.
    for(const std::unique_ptr<script>& owned : g_scripts) {
        script& entry = *owned;

        if(std::ranges::find(names, entry.file) == names.end()) {
            if(!entry.missing) {
                stop(entry);
                entry.enabled = false;
                entry.missing = true;
                ++gone;
                TW_LOG_INFO("lua: {} is gone from Scripts - unloaded", entry.file);
            }
            continue;
        }

        read_header(entry);

        if(entry.missing) {
            entry.missing = false;
            entry.enabled = tw::lua::config::enabled(entry.file);
            ++back;
            TW_LOG_INFO("lua: {} is back in Scripts{}", entry.file, entry.enabled ? "" : " - disabled, not loading");
            if(entry.enabled) {
                (void)launch(entry);
            }
        }
    }

    // New files go at the end, whatever their names: ids are indices, and an id never changes.
    for(std::size_t i = 0; i < files.size(); ++i) {
        if(find(names[i]) != nullptr) {
            continue;
        }

        g_scripts.push_back(catalogue(files[i], static_cast<int>(g_scripts.size())));
        script& entry = *g_scripts.back();
        ++added;
        TW_LOG_INFO("lua: {} is new in Scripts{}", entry.file, entry.enabled ? "" : " - disabled, not loading");
        if(entry.enabled) {
            (void)launch(entry);
        }
    }

    TW_LOG_INFO("lua: rescan - {} new, {} gone, {} back, {} script(s) in all", added, gone, back, g_scripts.size());
    changed();
}

void clear() noexcept
{
    g_scripts.clear();
    g_directory.clear();
    changed();
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

    return g_scripts[static_cast<std::size_t>(id)].get();
}

script* find(std::string_view file) noexcept
{
    for(const std::unique_ptr<script>& entry : g_scripts) {
        if(entry->file == file) {
            return entry.get();
        }
    }

    return nullptr;
}

int loaded_count() noexcept
{
    int loaded = 0;
    for(const std::unique_ptr<script>& entry : g_scripts) {
        if(entry->enabled && !entry->failed) {
            ++loaded;
        }
    }

    return loaded;
}

bool set_enabled(int id, bool enabled) noexcept
{
    script* const entry = find(id);
    if(entry == nullptr || entry->missing || tw::lua::vm::state() == nullptr) {
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
        stop(*entry);
        entry->enabled = false;

        tw::lua::config::set_enabled(entry->file, false);
        tw::lua::config::save();
        changed();

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

    // A toggle that did not take springs back - the user asked for "on" and did not get it, and
    // scripts.cfg keeps whatever it said before.
    if(!launch(*entry)) {
        entry->enabled = false;
        changed();
        return false;
    }

    tw::lua::config::set_enabled(entry->file, true);
    tw::lua::config::save();
    changed();

    TW_LOG_INFO("lua: enabled {}", entry->file);
    return true;
}

void reload(int id) noexcept
{
    script* const entry = find(id);
    if(entry == nullptr || entry->missing || tw::lua::vm::state() == nullptr) {
        return;
    }

    // Off stays off: a fresh header for the tab, nothing run.
    if(!entry->enabled) {
        read_header(*entry);
        changed();
        return;
    }

    tw::lua::sched::rearm();
    stop(*entry);
    read_header(*entry);

    if(launch(*entry)) {
        TW_LOG_INFO("lua: reloaded {}", entry->file);
    }
    else {
        TW_LOG_INFO("lua: reloaded {} - it failed: {}", entry->file, entry->error);
    }

    changed();
}

std::uint32_t revision() noexcept
{
    return g_revision;
}

void changed() noexcept
{
    ++g_revision;
}
} // namespace tw::lua::registry
