#pragma once

// Per-instance interception of Quest3D channel calls.
//
// The mechanism, and why it is this one rather than a Detour (Docs/Internal/lua-scripting.md §3.1):
// a detour on Aco_QueueChannel::CallChannel would fire for all 8223 ChannelCaller channels in the
// project and pay a trampoline plus a filter on every one of them, every frame. What is actually
// wanted is "tell me when *this* channel is called", and the engine's own object layout gives that
// away for free - single inheritance, one vptr at offset 0, objects on the heap, vtables in .rdata.
//
// So: copy the type's vtable into our own memory, replace slot 1 (CallChannel), and point one
// object's vptr at the copy. Channels nobody asked about are not touched and pay exactly nothing.
//
// The binding pointer is stashed immediately *before* the copied table, so the thunk recovers its
// context with a single load instead of a hash lookup:
//
//     [-2] binding*        <- ours
//     [-1] RTTI locator    <- copied verbatim, in case anything ever does a dynamic_cast
//     [ 0] destructor      <- copied
//     [ 1] CallChannel     <- ours
//     [ n] ...             <- copied
//
// Everything here runs on the engine thread (reversing-journal-engine.md §4.1), so there is no
// locking anywhere in this file.
namespace tw::framework::channel_shim
{
enum class phase {
    before, // the channel is about to run - values are still the previous frame's
    after,  // the original has returned - this is where a Do_* handler's results are readable
};

// Called on the engine thread, inside the game's own call stack. Keep it short and non-throwing.
//
// The return value is only consulted on `phase::before`, where **false means "do not run the
// original"** - the engine's own handler is skipped entirely, and the `after` phase is skipped with
// it (there is no "after" for something that did not happen). Returning true is the ordinary case
// and leaves the call untouched.
//
// Suppression is what makes this an interception point rather than an observation point: a channel
// is the unit the game's own logic is built out of, so declining to run one removes exactly that
// piece of behaviour - a render node stops drawing, a Do_* handler stops firing - without patching
// any code. What it cannot do is make the removal *safe*: a channel that another channel's result
// depends on will simply hand back whatever it held last. Choosing a node whose only effect is the
// one being removed is the caller's job, not this layer's.
using call_hook_fn = bool (*)(A3d_Channel* channel, void* user, phase when);

// Adds a subscriber to `channel`'s CallChannel, installing the vtable copy if this is the first one.
//
// **Several subscribers per channel are expected, not exceptional.** A user who installs a handful
// of scripts and turns them all on will have two of them watching the same popular Do_* handler
// sooner or later, and neither has any way to know about the other. So subscribers stack, and the
// dispatch rules are:
//
//  - every `before` subscriber runs, in subscription order, *before* any of them can cancel the
//    call. An observer therefore always sees the event regardless of what else is attached;
//  - if any of them returned false, the original is skipped - suppression is a logical OR, so two
//    scripts muting the same node compose, and turning one off still leaves it muted;
//  - a suppressed call has no `after` phase at all. An `after` subscriber exists to read what the
//    handler wrote, and nothing was written.
//
// `user` identifies the subscriber and must be unique per subscription - it is the key unsubscribe()
// takes. False when the channel's vtable cannot be copied safely (see the region clamp in the .cxx),
// or on allocation failure.
bool subscribe(A3d_Channel* channel, call_hook_fn hook, void* user) noexcept;

// Drops one subscriber. When it was the last one on that channel, the original vptr goes back and
// the copy is freed - so a channel nobody is watching any more costs the game nothing at all, which
// is what makes disabling a script genuinely free rather than merely quiet.
void unsubscribe(A3d_Channel* channel, void* user) noexcept;

// Drops every subscription whose `user` pointer is in the given set, wherever it is attached. Used
// to unload one script without walking its subscriptions channel by channel from the outside.
void unsubscribe_all_of(std::span<void* const> users) noexcept;

// Puts a channel's original vptr back and frees the copy, dropping every subscriber on it. Safe on
// a channel that was never hooked.
void remove(A3d_Channel* channel) noexcept;

// Unhooks everything. Called when the scripting layer reloads, and it must also be called before the
// DLL could ever be unloaded: the thunk lives in our image, so a channel still pointing at it after
// that would jump into freed memory.
void remove_all() noexcept;

// How many channels currently carry a vtable copy. Not the number of subscribers - several can
// share one channel.
[[nodiscard]] int active_count() noexcept;

// How many subscribers are attached to one channel. Zero for an unhooked one.
[[nodiscard]] int subscriber_count(A3d_Channel* channel) noexcept;
} // namespace tw::framework::channel_shim
