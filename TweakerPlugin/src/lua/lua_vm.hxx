#pragma once

// The LuaJIT state, and nothing that knows what a script is for.
//
// Owns the lua_State, runs the prelude (lua_prelude) against the C entry points - handed over **by
// name**, from the one list in lua_vm.cxx - strips the sandbox, and keeps references to the handful
// of prelude functions C calls back into. Running a script file is here too, because it is the one
// place that sets up a script's environment; deciding *which* scripts run is lua_registry's, and
// calling their callbacks is lua_sched's.
//
// Engine/render thread only (lua-scripting.md §7).
namespace tw::lua::vm
{
// Creates the state and runs the prelude. False, with last_error() saying why, when LuaJIT could not
// start or the prelude refused to - both leave the module inert rather than half-built.
bool initialize() noexcept;

void shutdown() noexcept;

// Null when initialize() has not succeeded.
[[nodiscard]] lua_State* state() noexcept;

// The prelude functions C calls, as the prelude's `exports` table names them.
enum class fn {
    dispatch_frame,
    dispatch_tick,
    dispatch_post_tick,
    dispatch_call,
    dispatch_state,
    dispatch_graph,
    unload_owner,
    run_script,
};

// Pushes that function. False (and nothing pushed) when the VM is not up.
bool push(fn which) noexcept;

// Pushes debug.traceback as captured before the sandbox strip, and returns its stack index - or 0
// when there is none, which lua_pcall reads as "no handler", so a caller needs no special case.
int push_traceback() noexcept;

// Runs `path` as script `owner`: its own environment chained to _G, __tw_owner stamped into it, the
// body run by the prelude with the registration window open. On failure `error` holds the message
// with its traceback.
bool run_file(const std::filesystem::path& path, int owner, std::string& error) noexcept;

// The last VM-level failure - LuaJIT could not start, the prelude did not load. Not a script's error:
// those belong to the script (lua_script, lua_diag).
void set_error(std::string_view where, const char* detail) noexcept;
[[nodiscard]] std::string_view last_error() noexcept;
} // namespace tw::lua::vm
