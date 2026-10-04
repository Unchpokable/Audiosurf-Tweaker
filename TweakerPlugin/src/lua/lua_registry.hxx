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

// Reads the directory load_all() was given again (script-manager-roadmap.md §9). New files are
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

// Bumped by everything that can change what a script's state looks like from outside - switching,
// loading, failing, a rescan, a suspension or a Resume (lua_sched). lua_remote compares it once a
// frame and looks at the scripts only when it moved.
[[nodiscard]] std::uint32_t revision() noexcept;
void changed() noexcept;
} // namespace tw::lua::registry
