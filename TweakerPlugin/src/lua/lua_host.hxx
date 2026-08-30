#pragma once

// The LuaJIT virtual machine embedded in the overlay, and the scripts running on it.
//
// Scope of this first cut is deliberately narrow - Ф1 of Docs/Internal/lua-scripting.md, minus the
// parts that need machinery that does not exist yet: one VM, scripts loaded from disk at startup,
// one callback (`tw.on_frame`), read-only access to the channel graph, and drawing through the
// ImGui background draw list. No per-channel hooks, no writes to the game, no plugin-state views.
//
// Everything here runs on the engine/render thread and only there (lua-scripting.md §7).
namespace tw::lua::host
{
// Creates the VM, builds the sandboxed script environment, and loads every .lua found next to the
// DLL under scripts/. Safe to call when there is nothing to load - that is the normal case, and it
// leaves the module inert rather than failing.
void initialize() noexcept;

void shutdown() noexcept;

// Runs every registered on_frame handler. Must be called from inside an ImGui frame, on the render
// thread - it draws. Wrapped internally in ImGui's own error-recovery state save/restore, so a
// script that dies mid-draw loses its own output rather than corrupting the overlay's ImGui stacks
// (lua-scripting.md §8.3, "Контейнер отказа").
void draw_frame() noexcept;

// Runs the Lua callback registered for a tw.on_call subscription.
//
// Called from the engine's own call stack, through framework/channel_shim - not from the render
// path, so there is no ImGui state to protect here, only the usual error containment. A callback
// that throws is disabled rather than left to fire again on the next channel call, which for a
// per-block event would be several times a second.
//
// Returns whether the engine's own handler should still run: false only when the callback explicitly
// returned `false`. Everything else - no return value, nil, an error, a disabled dispatcher - means
// proceed, so a script can never take a piece of the game away by accident.
[[nodiscard]] bool dispatch_call(int subscription_id) noexcept;

// Whether a VM exists at all. False when LuaJIT failed to start, which is survivable: the rest of
// the overlay is unaffected.
[[nodiscard]] bool is_running() noexcept;

// How many script files loaded without error.
[[nodiscard]] int loaded_script_count() noexcept;

// What one script file is and what it is currently doing. Every .lua found in scripts/ gets an entry,
// whether or not it is running - the overlay's Scripts tab has to be able to list something the user
// turned off, which means the descriptive fields must be knowable **without executing the file**.
//
// Hence header annotations rather than a `tw.manifest{...}` call: the first comment block of the
// script is scanned for `-- @name`, `-- @author`, `-- @version`, `-- @description`. Same convention
// as the `@sky` annotations the shader loader already reads out of HLSL sources. Anything missing
// falls back to something honest (the file name, an empty string) rather than to a placeholder.
struct script_info {
    int id = -1; // stable for the session; also the subscription owner id
    std::string file;
    std::string name;
    std::string author;
    std::string version;
    std::string description;

    bool enabled = false;
    bool failed = false; // the last load attempt errored - `error` says how
    std::string error;

    int hooks = 0; // live channel subscriptions this script currently holds
};

[[nodiscard]] int script_count() noexcept;

// Null for an out-of-range index. The pointer is valid until the next enable/disable/reload.
[[nodiscard]] const script_info* script_at(int index) noexcept;

// Turns one script on or off, and persists the choice.
//
// Enabling **runs the file again from disk**, with a fresh environment: there is no dormant copy to
// wake up, because disabling genuinely tore everything down. That makes this the reload path too,
// and it means edits to a script take effect on the next toggle without restarting the game.
//
// Disabling drops the script's frame handlers and channel callbacks and restores the original vtable
// of every channel that thereby lost its last subscriber, so a disabled script costs the game
// nothing at all - not a dormant hook, not a branch. Channels other scripts are still watching keep
// their copies and keep working.
//
// Returns whether the script is enabled afterwards, which is not always what was asked: enabling can
// fail if the file no longer parses.
bool set_script_enabled(int id, bool enabled) noexcept;

// Re-runs an enabled script from disk. No-op on a disabled one.
void reload_script(int id) noexcept;

// How many distinct channels currently have more than one script attached. Zero is the ordinary
// case; anything else is worth showing the user, because two scripts sharing an interception point
// is invisible until one of them behaves oddly.
[[nodiscard]] int shared_channel_count() noexcept;

// The last error text, empty when there has been none. Points into module-owned storage; valid
// until the next error.
[[nodiscard]] std::string_view last_error() noexcept;
} // namespace tw::lua::host
