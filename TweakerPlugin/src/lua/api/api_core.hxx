#pragma once

// The layer's own part of the C ABI: logging and diagnostics, the engine's lifecycle, the frame
// clock, easing, the group roster - and the C half of the per-script guard the prelude wraps every
// callback in. See api_channels.hxx for why everything here looks like C.
namespace tw::lua::api
{
extern "C" {
// The per-script guard, as the prelude's dispatchers see it (lua_sched.hxx has the whole story).
//
// Every callback a script registered runs as
//
//     if tw_script_enter(owner, kind) ~= 0 then
//         local ok, err = xpcall(fn, traceback, ...)
//         tw_script_leave(owner, kind, ok and nil or err)
//     end
//
// `enter` answers whether that script may run right now - zero for one that is suspended or off -
// and starts its clock; `leave` stops the clock, and a non-null `error` is that script's failure,
// counted against its own budget and nobody else's. `kind` is tw::lua::callback.
int tw_script_enter(int owner, int kind) noexcept;
void tw_script_leave(int owner, int kind, const char* error) noexcept;

// A diagnostic from a script: tw.warn, tw.error and tw.pending, and the channel layer's own "no such
// channel". `level` is tw::lua::diag::level, `where` is the place it was said from ("hud.lua:88"),
// and the whole triple is deduplicated - see lua_diag.hxx for what reaches the notefeed and when.
void tw_diag(int owner, int level, const char* where, const char* message) noexcept;

// Writes to the plugin log. Stripped in release builds like every other TW_LOG_* call.
void tw_log(const char* message) noexcept;

// Raises a notefeed toast - the overlay's existing transient notification strip - on behalf of one
// script. Capped per script (lua_diag): a script that calls this every frame gets a few toasts and
// then a line in its own diagnostics, not a screen full of them.
void tw_notify(int owner, const char* message) noexcept;

// Whether the graph is reachable at all - false until the engine pointer has been captured, which
// with the frame spine in place means "the game has run its first frame" (engine/engine_control).
int tw_engine_ready() noexcept;

// Whether writes to the graph are currently accepted. An alias for tw_ready(): the write gate
// stopped being a thing of its own when engine::state took over answering "has the game come up".
int tw_can_write() noexcept;

// The lifecycle state as an integer (tw::engine::state::phase) and as a name - "detached",
// "booting", "starting", "ready", "busy". One observable value replacing the three questions
// (engine captured? groups settled? writes allowed?) a script used to have to ask separately.
int tw_state() noexcept;
const char* tw_state_name() noexcept;

// `ready` or `busy`: the graph is up and a script may run and write. This is the gate the scheduler
// holds user callbacks behind, which is what stops a script from drawing over the loading screen.
int tw_ready() noexcept;

// Changes exactly when a channel that could not be resolved might now resolve - a group appeared or
// went away. A handle compares this instead of counting frames, so a name that is not there yet
// costs nothing until something actually changes (lua-engine-fix-roadmap.md §5.2).
int tw_graph_revision() noexcept;

// Diagnostics for "my group did not resolve": how many groups are loaded, and what each one is
// called. The engine stores a full path, so what a script types ("Puzzle") matches neither that nor
// necessarily the pool name - being able to print the real list is what turns a silent miss into an
// obvious one.
int tw_group_count() noexcept;
const char* tw_group_name(int index) noexcept;

// Whether a group with that pool or file name is loaded right now, out of the registry roster - no
// engine call and no scan. This is what tw.on_group is built on: a script watches for its group
// rather than polling for its channel.
int tw_group_loaded(const char* name) noexcept;

// The frame number a script sees as `tw.frame` - engine frames, monotonic, with a fallback to draw
// frames while the spine has never ticked. Owned by lua_sched; see sched::frame().
int tw_frame() noexcept;

// Seconds elapsed since the previous frame, from the same source the overlay's own animations use
// (ui/widgets/detail/draw.hxx: dt_ms, which carries the sub-millisecond remainder forward instead
// of rounding it away - at the several thousand FPS an uncapped window reaches, rounding makes every
// animation run at a multiple of its intended speed).
float tw_dt() noexcept;

// One of tweeny's easing curves evaluated at `t`, clamped to [0, 1] and mapped to [0, 1].
//
// A pure function, deliberately: the alternative was exposing tweeny's tween objects, which carry
// state and destructors and would need a per-script handle pool with the same ownership machinery
// the channel subscriptions already have. A script keeping its own `t` and calling this needs none
// of that, and it disappears with the script for free. See Docs/Internal/lua-scripting.md.
//
// `curve` indexes tw_ease_name below; out of range is linear.
float tw_ease(int curve, float t) noexcept;

int tw_ease_count() noexcept;
const char* tw_ease_name(int index) noexcept;
}
} // namespace tw::lua::api
