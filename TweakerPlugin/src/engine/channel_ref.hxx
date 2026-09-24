#pragma once

#include "engine/channel_kind.hxx"
#include "engine/engine_groups.hxx"

// A resolved channel, and everything that has to be known about it before a family-specific slot
// may be called.
//
// **Why a struct and not an `A3d_Channel*`.** A bare channel pointer answers none of the questions
// that decide whether calling through it is safe: which family it is (slot 17 means six different
// things, reversing-journal-boot.md §7.1), whether its group is still loaded (the `Renderer` pool is
// destroyed and rebuilt on every run, §6.1), whether it memoises at all (CHIC, §3.1). Before Ф3 the
// answers lived in comments - "the kind must already be known good, this does not re-check" - and in
// the discipline of every caller. Now they live in the value, and the family functions under
// src/engine/family/ accept nothing else.
namespace tw::engine
{
struct channel_ref {
    A3d_Channel* channel = nullptr;

    // The group the channel was found in, and that group's serial number at the time. The pair is
    // what valid() checks: a pointer comparison alone cannot tell a group from a new one that the
    // allocator happened to put at the same address.
    A3d_ChannelGroup* group = nullptr;
    groups::generation gen = groups::no_generation;

    // Position inside the group, for re-resolving after the group comes back. -1 when unknown.
    int index = -1;

    // Decided once, at resolve, from the channel's own ChannelType. The family functions refuse a ref
    // whose family is not theirs, so a mistake here is a refused call rather than a wild one.
    kind family = kind::unknown;

    // CHIC (A3d_Channel +0x60) != 0: the engine never memoises this channel, so its
    // channelCalculatedAtCount_ (+0x10) is never written and live() has no answer to give. 92.8 % of
    // Array Value channels are like this.
    bool no_memo = false;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return channel != nullptr;
    }
};

// Why a resolve did not produce a ref. The split between "not yet" and "wrong" is the whole point:
// the first is a normal state that stays silent, the second is a mistake in whatever asked and is
// reported once. The numeric values are part of the scripting ABI (lua_api.hxx: resolve_status).
enum class resolve_status : int {
    ok = 0,
    engine_pending = 1, // the engine pointer has not been captured yet
    no_group = 2,       // group not loaded - transient, groups load and unload
    no_channel = 3,     // group loaded, no such channel - final, a group's channels are fixed at load
    wrong_kind = 4,     // the channel exists and is a different family - never transient
    unusable = 5,       // right family, but the slot is not code. Should not happen
};

// Whether a channel's evaluation can be observed, and what it says. See channels::live().
enum class liveness : int {
    no = 0,
    yes = 1,
    unknown = -1, // CHIC = 1: the field that would say is never written
};
} // namespace tw::engine

namespace tw::engine::channels
{
// Whether channels can be resolved at all: the group registry is up and the engine pointer has been
// captured. Everything below returns engine_pending / an empty ref otherwise.
[[nodiscard]] bool available() noexcept;

// A group, through the registry's roster: full stored file name, pool name, or bare file name.
[[nodiscard]] A3d_ChannelGroup* find_group(const char* name) noexcept;

// A channel inside a group, by name or by index, with no family check. For the one consumer that
// genuinely does not care what a channel is: hooking its CallChannel, which is slot 1 of the base
// vtable and therefore the same on every family.
//
// **Names are not unique within a group.** TrafficCommander has two Values called "TrafficType"
// (#631 and #900), and a by-name lookup returns whichever comes first in scan order. When a name is
// ambiguous, the index is the only way to say which one is meant.
[[nodiscard]] A3d_Channel* find_channel(A3d_ChannelGroup* group, const char* name) noexcept;
[[nodiscard]] A3d_Channel* find_channel_at(A3d_ChannelGroup* group, int index) noexcept;

// Resolves a channel of an expected family into `out`. Cold path: a by-name lookup is a linear
// _stricmp scan over the whole group, several thousand channels in XX_StartHere, so a caller keeps
// the ref rather than resolving per frame.
//
// `actual`, when not null, receives the family the channel turned out to be - which is what a
// wrong_kind message needs to say both sides of the mistake.
resolve_status resolve(const char* group, const char* name, kind want, channel_ref& out, kind* actual = nullptr) noexcept;
resolve_status resolve_at(const char* group, int index, kind want, channel_ref& out, kind* actual = nullptr) noexcept;

// A ref for a channel pointer the caller already holds, with the family and CHIC read off the
// object. No group, so valid() is false for it - it exists for the offline suites, which build fake
// channels, and for code that got a channel from somewhere other than a resolve.
[[nodiscard]] channel_ref adopt(A3d_Channel* channel) noexcept;

// Whether the ref's group is still the one it was resolved in. A roster scan - cheap, but not free,
// so it is not done per read: callers compare groups::revision() and call this only when it moved.
[[nodiscard]] bool valid(const channel_ref& ref) noexcept;

// Whether the engine evaluated this channel in the **current** frame of its group.
//
// Two loads: channelCalculatedAtCount_ (+0x10) against A3d_ChannelGroup::GetTreeCalculateCount(),
// the exact pair CheckRenderCount compares to decide whether to memoise (boot journal §3.1). Equality
// only - the counter rings at 30000.
//
// **What it cannot mean, and why get() does not use it.** It is "this frame", not "ever": read from
// on_tick, which runs before the graph, it is false for everything. And any reader - ours included -
// that goes through a memoising getter writes the field itself, so it answers "was it evaluated",
// not "was it evaluated by the game". It is an instrument for asking whether a branch of the graph
// is running right now, and it is exposed as exactly that.
[[nodiscard]] liveness live(const channel_ref& ref) noexcept;

// Invalidates the channel's per-frame memo so the next read re-evaluates - but only when it has one.
// Writing +0x10 on a CHIC channel would be scribbling on a field the engine never reads for it.
void bust_memo(const channel_ref& ref) noexcept;

// The channel's name field, through the exported one-instruction accessor. May return null.
[[nodiscard]] const char* name_of(A3d_Channel* channel) noexcept;
} // namespace tw::engine::channels
