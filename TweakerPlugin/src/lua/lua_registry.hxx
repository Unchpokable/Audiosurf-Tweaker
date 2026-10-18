#pragma once

#include "lua/lua_script.hxx"

// The set of scripts: what is in Scripts\, which of it is switched on, and switching.
//
// A script's id is its index here, fixed for the session: the list is built sorted by file name, only
// ever grows at the end (rescan()), and is never reordered or shrunk - a file that disappears stays a
// record, marked missing. That is what lets the scheduler answer "may this script run" with one bounds
// check and one load on every hooked channel call.
//
// Each script lives on the heap and the list holds owning pointers to them, so growing the list moves
// the pointers and never the scripts: what find() returns stays valid for the session, across rescans.
//
// Engine/render thread only.
namespace tw::lua::registry
{
// Catalogues every .lua in `directory` - header only, nothing executed - then runs the enabled ones.
// Two passes because the tab has to be able to list a script the user turned off, and that listing
// comes from the file's header rather than from anything the file does when it runs.
void load_all(const std::filesystem::path& directory) noexcept;

// Reads the directory load_all() was given again (script-manager.md §8.9). New files are
// appended - and run, unless scripts.cfg has them off; files that are gone are unloaded and marked
// missing; files that came back run again if scripts.cfg says so; every header is read again. Nothing
// already running is restarted - that is reload().
//
// Not per-frame work: it runs when something was installed or removed, or when somebody asked.
void rescan() noexcept;

void clear() noexcept;

[[nodiscard]] int count() noexcept;

// Null for an id that is not a script (including -1, the layer). The pointer is stable for the
// session.
[[nodiscard]] script* find(int id) noexcept;

// By file name in Scripts\ (`puzzlepro_hud.lua`), case-sensitive like scripts.cfg. Missing records are
// found too - they are still scripts, only without a file.
[[nodiscard]] script* find(std::string_view file) noexcept;

// How many scripts are running - enabled and loaded without error.
[[nodiscard]] int loaded_count() noexcept;

// Turns one script on or off, and persists the choice.
//
// Enabling **runs the file again from disk**, with a fresh environment and a clean health record:
// there is no dormant copy to wake up, because disabling genuinely tore everything down. Edits to a
// script take effect on the next toggle without restarting the game.
//
// Disabling runs the script's on_unload, drops its callbacks and restores the original vtable of
// every channel that thereby lost its last subscriber, so a disabled script costs the game nothing
// at all - not a dormant hook, not a branch. Channels other scripts are still watching keep their
// copies and keep working.
//
// Returns whether the script is enabled afterwards, which is not always what was asked: enabling can
// fail if the file no longer parses, and a missing script cannot be enabled at all.
bool set_enabled(int id, bool enabled) noexcept;

// Re-runs an enabled script from disk; on a disabled one, reads its header again and runs nothing.
//
// Unlike set_enabled() it persists nothing, and a script that fails here stays switched on - failed,
// the way one that fails at startup does. Reload is what a rebuild sends: a broken build must not
// quietly turn the script off in scripts.cfg, or the fixed build after it would not run.
void reload(int id) noexcept;

// **The order scripts run in** (Docs/Internal/script-coexistence.md §3) - ids, first to run
// first. Separate from the id on purpose: the id is what everything holds for the session, the place
// is what the user changes by dragging a row.
//
// Set by the user and nobody else, and kept in scripts.cfg as `order=` lines. At startup: the scripts
// the file lists, in its order, then the rest by file name. A file a rescan finds goes last, a missing
// one keeps its place, and nothing a script does - reload, toggle, failure - moves it. Every list that
// runs scripts one after another follows it: the bodies at startup, each of the prelude's callback
// lists, and the subscribers of each hooked channel (lua_sched::reorder).
[[nodiscard]] std::span<const int> order() noexcept;

// Where script `id` runs, 0 first; -1 for an id that is not a script (including -1, the layer).
[[nodiscard]] int place_of(int id) noexcept;

// Moves one script to `place` - its index in the order once it is there, clamped - and puts the new
// order into effect at once: scripts.cfg is written, and every running list is re-sorted. Cold: a drag.
void move(int id, int place) noexcept;

// The whole order from file names, as TweakerUI sends it: the names given, in that order, then every
// script they leave out, in the order it had. Unknown names are ignored.
void set_order(std::span<const std::string> files) noexcept;

// Bumped by everything that can change what a script's state looks like from outside - switching,
// loading, failing, a rescan, a reorder, a suspension or a Resume (lua_sched). lua_remote compares it once a
// frame and looks at the scripts only when it moved.
[[nodiscard]] std::uint32_t revision() noexcept;
void changed() noexcept;
} // namespace tw::lua::registry
