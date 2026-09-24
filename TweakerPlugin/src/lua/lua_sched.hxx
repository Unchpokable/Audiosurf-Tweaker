#pragma once

// When scripts run, and what happens when one of them misbehaves (lua-engine-fix-roadmap.md §6).
//
// **Where the per-script isolation actually lives - and why it is split the way it is.** The
// dispatch loops are Lua, in the prelude (tw.callbacks): the lists of handlers are Lua tables, and
// walking them from C would mean a C-to-Lua transition per handler for nothing. Each handler runs
// there under its own xpcall. What cannot live in Lua is the bookkeeping - a clock that has to be
// cheap and trustworthy, the budgets, the suspension that C also has to see (a mute never enters the
// VM) - so every guarded call brackets itself with enter() and leave() below, through the FFI.
//
// There is deliberately **no ImGui recovery snapshot per script**, which the roadmap first asked
// for. It would protect against a script leaving an ImGui stack pushed, and no script can: every
// tw.hud.* call adds to a draw list and returns, and a Lua error cannot land in the middle of a C
// function. One snapshot around the whole draw dispatch stays, as a backstop for the layer itself -
// in lua_host::draw_frame, so that nothing in here needs ImGui (and harness/sched can build it).
//
// The budgets:
//
//  - **errors**: five in 300 engine frames suspends the script. One error is a bad frame; five in a
//    few seconds is a script that is failing faster than it is working;
//  - **time**: per script, the moving average of its callbacks' cost per engine frame. Above 0.5 ms
//    the Scripts tab shows it; above 4 ms for 120 frames in a row the script is suspended. Frames in
//    the `busy` state are not counted - the game is loading a run there and every frame is slow, so
//    measuring somebody's script against them would be measuring the game.
//
// Engine frames are the right unit because in `ready` the engine and the overlay run 1:1 - Present is
// inside the graph (reversing-journal-boot.md §1.2, §10); the only place the graph free-runs is while
// it evaluates without presenting, which is loading, which is excluded. A session where the spine did
// not install measures per overlay frame instead.
//
// A suspended script is inert, not unloaded: its handlers stay registered and skip, its on_call
// hooks let the game's handler run, **its mutes let the call through**. Resume puts it back exactly
// as it was; Reload runs it from disk.
//
// Engine/render thread only.
namespace tw::lua::sched
{
// Subscribes to the engine spine. Called once the VM is up.
void initialize() noexcept;

void shutdown() noexcept;

// The overlay's frame: lifecycle notifications, then on_frame. Must be called from inside an ImGui
// frame - it draws.
void draw_frame() noexcept;

// The engine's frame, before and after the graph (engine/engine_control): lifecycle, on_ready,
// on_tick; then on_post_tick. Nothing here may touch ImGui.
void tick_frame() noexcept;
void post_tick_frame() noexcept;

// Runs the Lua callback for a tw.on_call subscription, from the engine's own call stack through
// framework/channel_shim. Returns whether the engine's handler should still run: false only when a
// "before" callback explicitly returned `false`. Everything else - no return value, an error, a
// suspended script - means proceed, so a script can never take a piece of the game away by accident.
[[nodiscard]] bool dispatch_call(int subscription_id) noexcept;

// The C half of the per-script guard (see the header comment, and tw_script_enter/leave).
//
// enter() answers whether `owner` may run a callback of this kind now and starts its clock; leave()
// stops the clock and, with a non-null `error`, files the failure against that script. Owner -1 is
// not a script: always allowed, never measured.
[[nodiscard]] bool enter(int owner, int kind) noexcept;
void leave(int owner, int kind, const char* error) noexcept;

// Whether a script's hooks are live - enabled and not suspended. Owner -1 always is. One indexed
// load: this is asked on every call of every hooked channel.
[[nodiscard]] bool runnable(int owner) noexcept;

// Takes a suspended script back into service with a clean budget. False if it was not suspended.
bool resume(int owner) noexcept;

// Runs one script's on_unload handlers and forgets everything it registered, on both sides of the
// boundary.
void unload(int owner) noexcept;

// The frame number scripts see as tw.frame: engine frames once the spine has ticked, overlay frames
// before that, and never backwards across the handover.
[[nodiscard]] std::uint32_t frame() noexcept;

// The limits above, for the Scripts tab to compare against.
[[nodiscard]] float soft_budget_ms() noexcept;

// A dispatcher itself - not a script - failed. That is a bug in the prelude, not in anybody's script,
// so it cannot be pinned on a script; the dispatcher stays off until the set of running scripts
// changes. These re-arm it.
void rearm() noexcept;
} // namespace tw::lua::sched
