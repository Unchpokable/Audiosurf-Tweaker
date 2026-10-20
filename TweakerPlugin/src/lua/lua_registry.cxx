#include "pch.hxx"

#include "lua/lua_registry.hxx"

#include "lua/api/api_hooks.hxx"
#include "lua/lua_config.hxx"
#include "lua/lua_diag.hxx"
#include "lua/lua_log.hxx"
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

// The order scripts run in: ids, first to run first (script-coexistence.md §3). Every record is
// in it exactly once, missing ones included - a file that went away keeps its place for when it comes
// back. g_place is the inverse, by id, so "where does this script run" is one load.
std::vector<int> g_order;
std::vector<int> g_place;

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
//
// `verb` is what the script log calls it - loaded, switched on, reloaded. The error itself is already there, in
// full, from lua_diag; this line says what became of the script.
bool launch(tw::lua::script& script, std::string_view verb) noexcept
{
    if(run(script)) {
        tw::lua::log::event(script.id, verb);
        return true;
    }

    tw::lua::sched::unload(script.id);
    tw::lua::log::writef(script.id, "script", {}, "{}: failed, not running", verb);
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

// "puzzlepro_hud.lua, particles.lua" - the run order, for the script log.
std::string order_text()
{
    std::string text;
    for(const int id : g_order) {
        if(!text.empty()) {
            text += ", ";
        }
        text += g_scripts[static_cast<std::size_t>(id)]->file;
    }

    return text.empty() ? std::string { "(none)" } : text;
}

void index_places() noexcept
{
    g_place.assign(g_scripts.size(), -1);
    for(std::size_t place = 0; place < g_order.size(); ++place) {
        g_place[static_cast<std::size_t>(g_order[place])] = static_cast<int>(place);
    }
}

// The order at startup: what scripts.cfg lists, in its order, then everything it does not, by id - which
// at startup is by file name. Names in the file with no script behind them are skipped here and dropped at
// the next write.
void initial_order() noexcept
{
    g_order.clear();
    g_order.reserve(g_scripts.size());

    for(const std::string& name : tw::lua::config::order()) {
        for(const std::unique_ptr<tw::lua::script>& entry : g_scripts) {
            if(entry->file == name) {
                g_order.push_back(entry->id);
                break;
            }
        }
    }

    for(const std::unique_ptr<tw::lua::script>& entry : g_scripts) {
        if(std::ranges::find(g_order, entry->id) == g_order.end()) {
            g_order.push_back(entry->id);
        }
    }

    index_places();
}

// Puts a new order into effect: scripts.cfg, then everything that runs scripts one after another. The
// registry's own idea comes first, so that the prelude and the hook table, asking where each script
// runs while they re-sort, already get the new answer.
void apply(std::vector<int> order) noexcept
{
    if(order == g_order) {
        return;
    }

    g_order = std::move(order);
    index_places();

    std::vector<std::string> names;
    names.reserve(g_order.size());
    for(const int id : g_order) {
        names.push_back(g_scripts[static_cast<std::size_t>(id)]->file);
    }
    tw::lua::config::set_order(std::move(names));
    tw::lua::config::save();

    tw::lua::sched::reorder();
    tw::lua::registry::changed();

    TW_LOG_INFO("lua: run order changed - {} script(s)", g_order.size());
    tw::lua::log::write(-1, "script", {}, "run order: " + order_text());
    tw::lua::log::flush();
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

    initial_order();

    tw::lua::log::write(-1, "script", {}, std::format("{} script(s) in Scripts, run order: {}", g_scripts.size(), order_text()));

    // In run order, so that what one body does at the top level - a notification, a log line - comes
    // out in the order everything else will run in.
    for(const int id : g_order) {
        const std::unique_ptr<script>& entry = g_scripts[static_cast<std::size_t>(id)];
        if(!entry->enabled) {
            TW_LOG_INFO("lua: {} is disabled - not loading", entry->file);
            tw::lua::log::event(entry->id, "switched off in scripts.cfg - not loaded");
            continue;
        }

        // A script that fails here stays switched on, so that the next toggle or reload retries it.
        if(launch(*entry, "loaded")) {
            TW_LOG_INFO("lua: loaded {}", entry->file);
        }
    }

    tw::lua::log::flush();
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
                tw::lua::log::event(entry.id, "gone from Scripts - unloaded");
            }
            continue;
        }

        read_header(entry);

        if(entry.missing) {
            entry.missing = false;
            entry.enabled = tw::lua::config::enabled(entry.file);
            ++back;
            TW_LOG_INFO("lua: {} is back in Scripts{}", entry.file, entry.enabled ? "" : " - disabled, not loading");
            tw::lua::log::event(entry.id, entry.enabled ? "back in Scripts" : "back in Scripts - switched off, not loaded");
            if(entry.enabled) {
                (void)launch(entry, "loaded");
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

        // Last to run, too, before it runs: its registrations ask where it runs. Not written to
        // scripts.cfg - nobody chose this place, and a script that is not listed runs after the listed
        // ones anyway, which is where it is.
        g_order.push_back(entry.id);
        index_places();

        ++added;
        TW_LOG_INFO("lua: {} is new in Scripts{}", entry.file, entry.enabled ? "" : " - disabled, not loading");
        tw::lua::log::event(entry.id, entry.enabled ? "new in Scripts" : "new in Scripts - switched off, not loaded");
        if(entry.enabled) {
            (void)launch(entry, "loaded");
        }
    }

    TW_LOG_INFO("lua: rescan - {} new, {} gone, {} back, {} script(s) in all", added, gone, back, g_scripts.size());
    tw::lua::log::write(
        -1, "script", {}, std::format("rescan: {} new, {} gone, {} back, {} script(s) in all", added, gone, back, g_scripts.size()));
    tw::lua::log::flush();
    changed();
}

void clear() noexcept
{
    g_scripts.clear();
    g_order.clear();
    g_place.clear();
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
        tw::lua::log::event(entry->id, "switched off");
        tw::lua::log::flush();
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
    if(!launch(*entry, "switched on")) {
        entry->enabled = false;
        tw::lua::log::flush();
        changed();
        return false;
    }

    tw::lua::config::set_enabled(entry->file, true);
    tw::lua::config::save();
    tw::lua::log::flush();
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
        tw::lua::log::event(entry->id, "reload: switched off - header read again, nothing run");
        tw::lua::log::flush();
        changed();
        return;
    }

    tw::lua::sched::rearm();
    stop(*entry);
    read_header(*entry);

    if(launch(*entry, "reloaded")) {
        TW_LOG_INFO("lua: reloaded {}", entry->file);
    }
    else {
        TW_LOG_INFO("lua: reloaded {} - it failed: {}", entry->file, entry->error);
    }

    // Now rather than within the writer's tenth of a second: a reload is what an author's save sets off, and if
    // the new code takes the game down on its first frame, this is the last thing the log should have.
    tw::lua::log::flush();
    changed();
}

std::span<const int> order() noexcept
{
    return g_order;
}

int place_of(int id) noexcept
{
    if(id < 0 || id >= static_cast<int>(g_place.size())) {
        return -1;
    }

    return g_place[static_cast<std::size_t>(id)];
}

void move(int id, int place) noexcept
{
    const int from = place_of(id);
    if(from < 0) {
        return;
    }

    std::vector<int> order = g_order;
    order.erase(order.begin() + from);
    const int to = std::clamp(place, 0, static_cast<int>(order.size()));
    order.insert(order.begin() + to, id);

    apply(std::move(order));
}

void set_order(std::span<const std::string> files) noexcept
{
    std::vector<int> order;
    order.reserve(g_order.size());

    for(const std::string& name : files) {
        const script* const entry = find(name);
        if(entry != nullptr && std::ranges::find(order, entry->id) == order.end()) {
            order.push_back(entry->id);
        }
    }

    for(const int id : g_order) {
        if(std::ranges::find(order, id) == order.end()) {
            order.push_back(id);
        }
    }

    apply(std::move(order));
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
