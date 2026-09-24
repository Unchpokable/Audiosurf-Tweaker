#pragma once

#include "framework/detour_transaction.hxx"

// The registry of loaded channel groups: what is there, what just appeared, and - the part nothing
// else in the plugin could do - what is about to be destroyed.
//
// **Why a registry at all.** A group is the unit the engine loads and unloads, and it does so for
// the whole session, not only while the game is starting: `XX_StartHere::Do_ResetLevelContent`
// drops the `Renderer` pool and loads it again on every single run (boot journal §6.1). Everything
// the scripting layer caches - a channel pointer, a vtable copy stapled onto one object - belongs to
// a group, and until now nothing noticed when that group went away. A stale channel pointer is a
// wrong read; a vtable copy outliving its object is a jump into freed memory the next time the
// engine touches it.
//
// **How appearance is noticed: by counting.** There is no "group loaded" event to hook, and there
// does not need to be one - `GetChannelGroupCount()` is a single call, and one comparison per frame
// catches both directions (boot journal §6.3). The existing write gate in lua_api.cxx was already
// leaning on this, blindly; this makes it the layer everything else asks.
//
// **How disappearance is noticed: by detour, and it has to be before the fact.** `A3d_ChannelGroup::
// Release` is the wide path every destruction goes through, and `EngineInterface::DeleteChannelGroup`
// the narrow one the game's own `Remove Group` channel uses (boot journal §6.2). Both are detoured,
// and the work happens *before* the original runs, while the channels are still there to be unhooked.
// Doing it after would mean handing the engine's own destructor a vtable copy that is about to be
// freed - which is the one ordering the offline suite mutates to prove the check is real.
//
// Everything here runs on the engine thread: tick() from the frame spine, the unload hooks from
// inside the game's own call stack. Same rule as framework/channel_shim - no locking anywhere.
namespace tw::engine::groups
{
// A group's identity, and the thing that makes a cached pointer checkable. Handed out by
// generation_of() and handed back to alive(); never compared against anything but itself.
//
// It is a serial number, not a counter of unload events, and that distinction is the whole point:
// the allocator reuses addresses, so a group loaded at the same address as one that went away is a
// different group with a different generation, and any handle still holding the old number fails
// its check rather than silently addressing the new group's channels.
using generation = std::uint32_t;

// Never handed out for a real group, so it is safe as "no group" in a handle.
constexpr generation no_generation = 0;

// A group appeared or disappeared. Called from tick(), after the roster has been rebuilt, so the
// listener sees the new state - not from inside the unload detour.
using changed_fn = void (*)() noexcept;

// A group is about to be destroyed. Called from inside the game's Release/DeleteChannelGroup, with
// the group still intact and its channels still readable, before the original runs.
//
// **The listener must not call back into the engine.** The pointers it is given are alive, the
// object is not: it is mid-teardown, whatever it keeps in its own lists may already be half gone.
// Drop what you cached and return.
using unloading_fn = void (*)(A3d_ChannelGroup* group, generation gen) noexcept;

// Resolves the group tier of engine_symbols. False when one of those exports is absent, which leaves
// the frame spine working and everything here inert - see engine_symbols::groups_ready().
bool initialize() noexcept;

[[nodiscard]] bool available() noexcept;

// Detours Release and DeleteChannelGroup. Cold, once. The `threads` argument carries the same
// meaning it does in engine::control::install() - `none` under the loader lock, `others` otherwise.
bool install_hooks(framework::detour::suspend threads = framework::detour::suspend::others) noexcept;

[[nodiscard]] bool hooks_installed() noexcept;

// Subscribed to the frame spine's pre-graph phase. One engine call in the common case; the rebuild
// behind it is a cold path taken only when the count moved.
void tick() noexcept;

// How many groups the engine had at the last tick. Cheap: this is the cached number, not a call.
[[nodiscard]] int count() noexcept;

// Bumped every time the roster actually changed. A cheap "has anything moved since I looked".
[[nodiscard]] std::uint32_t revision() noexcept;

// The engine frame and the millisecond timestamp of the last roster change. Both, because neither is
// enough on its own: the engine's frame rate has been measured at 625 Hz during a load and at the
// screen's refresh rate in the menu, so a settle window counted in frames alone expires inside the
// very window it is supposed to wait out (lua-engine-fix-roadmap.md §4.1).
[[nodiscard]] std::uint32_t last_change_frame() noexcept;
[[nodiscard]] std::uint64_t last_change_ms() noexcept;

// The group `EngineLoop` itself calls this frame: `EngineInterfaceExt::GetStartGroup()` followed by
// `EngineInterface::GetChannelGroup(int)`, which is literally what the engine's own frame does
// (boot journal §1.2). Null before the engine pointer is captured, or when the ext pointer failed
// its identity check.
//
// **Not `GetQ3DStartGroup()`**, which loads the group from disk on a miss and puts up a MessageBox.
[[nodiscard]] A3d_ChannelGroup* start_group() noexcept;

// The start group's file name ("XX_StartHere.cgr" once the game has handed over, the loader's own
// file before that). Empty string, never null, when there is no start group. Module-owned.
[[nodiscard]] const char* start_group_file() noexcept;

// A group by pool name ("StatCollector") or bare file name ("Puzzle"), case-insensitive, out of the
// cached roster - no engine call and no scan of the engine's own list. Null when not loaded.
[[nodiscard]] A3d_ChannelGroup* find(const char* name) noexcept;

// The serial number of a group currently in the roster, or no_generation for one that is not.
[[nodiscard]] generation generation_of(A3d_ChannelGroup* group) noexcept;

// Whether that exact group is still loaded. This is the check a cached channel pointer is worth
// nothing without: a matching address with a different generation means the address came back and
// the group did not.
[[nodiscard]] bool alive(A3d_ChannelGroup* group, generation gen) noexcept;

// Names out of the roster, module-owned and valid until the next rebuild. Empty string, never null.
[[nodiscard]] const char* pool_name_of(A3d_ChannelGroup* group) noexcept;
[[nodiscard]] const char* file_name_of(A3d_ChannelGroup* group) noexcept;

// Roster enumeration, for diagnostics and for the "what is actually loaded" answer scripts ask for.
[[nodiscard]] int roster_size() noexcept;
[[nodiscard]] A3d_ChannelGroup* roster_at(int index) noexcept;

// Listener registration. Fixed capacity for the same reason the spine's is: these are plugin
// modules wired up during startup, not anything a script can add to.
void subscribe_changed(changed_fn fn) noexcept;
void subscribe_unloading(unloading_fn fn) noexcept;
} // namespace tw::engine::groups
