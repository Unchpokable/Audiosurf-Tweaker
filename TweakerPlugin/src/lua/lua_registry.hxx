#pragma once

#include "lua/lua_script.hxx"

// The set of scripts: what is in Scripts\, which of it is switched on, and switching.
//
// A script's id is its index here, fixed for the session: the list is built once, sorted by file
// name, and never reordered or shrunk. That is what lets the scheduler answer "may this script run"
// with one bounds check and one load on every hooked channel call.
//
// Engine/render thread only.
namespace tw::lua::registry
{
// Catalogues every .lua in `directory` - header only, nothing executed - then runs the enabled ones.
// Two passes because the tab has to be able to list a script the user turned off, and that listing
// comes from the file's header rather than from anything the file does when it runs.
void load_all(const std::filesystem::path& directory) noexcept;

void clear() noexcept;

[[nodiscard]] int count() noexcept;

// Null for an id that is not a script (including -1, the layer). The pointer is stable for the
// session.
[[nodiscard]] script* find(int id) noexcept;

// How many scripts are running - enabled and loaded without error.
[[nodiscard]] int loaded_count() noexcept;

// Turns one script on or off, and persists the choice.
//
// Enabling **runs the file again from disk**, with a fresh environment and a clean health record:
// there is no dormant copy to wake up, because disabling genuinely tore everything down. That makes
// this the reload path too, and it means edits to a script take effect on the next toggle without
// restarting the game.
//
// Disabling runs the script's on_unload, drops its callbacks and restores the original vtable of
// every channel that thereby lost its last subscriber, so a disabled script costs the game nothing
// at all - not a dormant hook, not a branch. Channels other scripts are still watching keep their
// copies and keep working.
//
// Returns whether the script is enabled afterwards, which is not always what was asked: enabling can
// fail if the file no longer parses.
bool set_enabled(int id, bool enabled) noexcept;

// Re-runs an enabled script from disk. No-op on a disabled one.
void reload(int id) noexcept;
} // namespace tw::lua::registry
