#pragma once

// Channel-call hooks: tw.on_call and tw.mute, and the subscription table behind both.
//
// The extern "C" half is what the prelude calls (see api_channels.hxx for why it looks like C). The
// C++ half below it is for the rest of src/lua/ - the registry takes a script's hooks back out when
// it is switched off, and the Scripts tab counts them.
namespace tw::lua::api
{
extern "C" {
// Subscribes to a channel's CallChannel. Any family is accepted - the Do_* handlers that carry the
// game's events are ChannelCaller channels, which have no accessor of their own.
//
// `after` selects the phase: non-zero means the callback runs once the original has returned, which
// is what an event consumer wants (the handler's writes have landed). `out` carries a resolve_status
// exactly as tw_channel_resolve does.
//
// Returns a subscription id >= 0, or -1. The id comes back to Lua as the argument of the prelude's
// dispatch_call export, which is how the callback is found again without C ever holding a Lua value.
// `owner` is the id of the script that asked, so the subscription can be taken back out when that
// script is disabled without disturbing anyone else's - see unsubscribe_owner. -1 means unowned.
//
// A group that is not loaded is not a failure: the subscription is kept and attached whenever the
// group appears - and again every time it is rebuilt. -1 means it can never happen: the group is
// loaded and has no such channel, or the channel could not be hooked. Both are reported to the
// script's diagnostics from here.
int tw_on_call(int owner, const char* group, const char* name, int after, int* out) noexcept;

// Same, by channel index.
int tw_on_call_at(int owner, const char* group, int index, int after, int* out) noexcept;

// Takes a channel out of the graph: its CallChannel is intercepted and the engine's own handler is
// never run. The channel keeps its place in the graph and its parents keep calling it - they simply
// get nothing back.
//
// This is `tw_on_call` with a "before" handler that always declines, minus the Lua round trip. It
// exists separately because the thing worth suppressing is usually a render node called once per
// frame forever, and crossing into the VM sixty times a second to answer "no" is pure cost.
//
// What it is safe on is entirely a question of which channel is picked: a node whose only effect is
// drawing can be removed with no consequence, while one whose result something else reads leaves
// that reader holding a stale value. Nothing here can tell the two apart.
//
// **A mute belongs to its script's health, not only to its switch.** While the owning script is
// suspended (lua_sched), the mute lets the call through: a HUD script that dies has to hand the
// game's own HUD back, or the player is left with neither.
//
// Returns a subscription id (the same space as tw_on_call, so clear_subscriptions releases both), or
// -1 with `out` carrying a resolve_status.
int tw_mute(int owner, const char* group, const char* name, int* out) noexcept;
int tw_mute_at(int owner, const char* group, int index, int* out) noexcept;

// Turns a mute on or off without unhooking. The vtable stays swapped either way - re-hooking per
// toggle would mean a fresh vtable copy each time, and toggling is exactly what a script does when
// the feature it replaces goes on and off.
void tw_mute_set(int id, int enable) noexcept;
}

// Drops every subscription belonging to one script, and restores the vtable of every channel that
// thereby lost its last subscriber. This is what "disable a script" means at this layer: not a
// dormant hook that returns early, but no hook at all, so a disabled script costs the game exactly
// nothing. Other scripts watching the same channels keep working.
void unsubscribe_owner(int owner) noexcept;

// Drops every subscription and puts the original vtables back.
void clear_subscriptions() noexcept;

// How many subscriptions exist, in total (owner < 0) or for one script.
[[nodiscard]] int subscription_count(int owner) noexcept;

// How many of one script's subscriptions are waiting for their group to load - accepted, but not
// attached to anything yet. Appends the names of the groups they wait for to `groups`, once each,
// when it is not null. Cold: the Scripts tab asks, once per row per frame while it is open.
[[nodiscard]] int waiting_count(int owner, std::vector<std::string>* groups) noexcept;

// How many distinct channels currently carry more than one subscription - i.e. in how many places
// two scripts are sharing an interception point.
[[nodiscard]] int shared_channel_count() noexcept;

// Wires the subscription table to the group registry: one listener that drops a subscription's
// pointers when its group is destroyed, and one that attaches it again when the group comes back.
// Called once from lua_host::initialize, after the engine layer is up.
void install_engine_listeners() noexcept;
} // namespace tw::lua::api
