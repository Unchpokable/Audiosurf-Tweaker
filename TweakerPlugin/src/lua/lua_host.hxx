#pragma once

// The scripting layer as the rest of the plugin sees it: bring it up, draw its frame, take it down.
//
// Everything else lives in the pieces this puts together (lua-engine-fix-roadmap.md §3.3):
//
//   lua_vm        the LuaJIT state, the prelude, the sandbox, the one list of C entry points
//   lua_prelude   the Lua half - `tw` itself, handles, registration, the dispatch loops
//   lua_registry  the scripts in Scripts\, switching them on and off
//   lua_script    one script: its header and its health
//   lua_sched     when callbacks run, per-script isolation, the error and time budgets
//   lua_diag      what the layer has to say about a script, deduplicated and routed
//   api/*         the C ABI the prelude binds, one file per area
//   lua_ui        the Scripts tab
//
// Everything here runs on the engine/render thread and only there (lua-scripting.md §7).
namespace tw::lua::host
{
// Creates the VM, builds the sandboxed script environment, and loads every .lua found in
// engine\TweakerStuff\Scripts (plugin/paths). Safe to call when there is nothing to load - that is the
// normal case, and it leaves the module inert rather than failing.
void initialize() noexcept;

void shutdown() noexcept;

// Runs every on_frame handler, each script under its own guard. Must be called from inside an ImGui
// frame, on the render thread - it draws.
void draw_frame() noexcept;

// Whether a VM exists at all. False when LuaJIT failed to start, which is survivable: the rest of
// the overlay is unaffected.
[[nodiscard]] bool is_running() noexcept;

// Why the VM is not running, empty when it is. A script's own errors are not here - they belong to
// the script (lua_diag).
[[nodiscard]] std::string_view last_error() noexcept;
} // namespace tw::lua::host
