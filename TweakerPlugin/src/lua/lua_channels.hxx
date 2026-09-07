#pragma once

// Typed access to the game's Quest3D channel graph, for the scripting layer.
//
// Everything here rests on the ABI recovered in Docs/Internal/reversing-journal-engine.md: a channel
// is an object whose first dword is a vtable pointer, and the slots past the 17-slot A3d_Channel
// base belong to whatever the channel derives from. Nothing is guessed at runtime.
//
// Two rules shape this interface.
//
// **Typed, not generic.** There is no "read a channel" - there is "read a float channel", "read a
// text channel". Slot 17 (+0x44) is GetFloat on an Aco_FloatChannel and GetString on an
// Aco_StringChannel, and on a type that adds no virtuals of its own it is past the end of the vtable
// entirely. A generic reader either guesses or refuses everything it cannot prove numeric; a typed
// one tells the caller it asked with the wrong accessor, which is the actual mistake.
//
// **Resolve is separate from read.** A lookup by name is a linear _stricmp scan over every channel in
// the group (~7000 of them in XX_StartHere) and must never happen per frame; a read is one indirect
// call. So callers resolve once, keep the handle, and read as often as they like.
namespace tw::lua::channels
{
// The channel families the engine's type system actually distinguishes, keyed off ChannelType's base
// guid (see reversing-journal-engine.md §3.2). These six cover 82 of the 226 types in channels.lst;
// the rest are their own roots and report `other`.
enum class kind : int {
    unknown = -1, // could not be determined - no type record, or the object is not usable
    number = 0,   // Aco_FloatChannel: Value, Expression Value, Trigger, Array Value, Lua Script, ...
    text = 1,     // Aco_StringChannel: Text, Array Text, TextOperator, ...
    vector = 2,
    matrix = 3,
    texture = 4,
    object = 5,
    other = 6, // a known channel of a family this layer has no accessor for
};

[[nodiscard]] const char* kind_name(kind value) noexcept;

// Resolves the exported entry points out of HighPoly.dll. Safe to call repeatedly; only the first
// call does work. False means the module is not mapped yet, which is a legitimate "try again later"
// rather than an error.
bool initialize() noexcept;

[[nodiscard]] bool is_ready() noexcept;

// Whether the engine pointer has been captured yet (framework/channel_hook). Until it has, every
// resolve below returns null - see Docs/Internal/lua-scripting.md §7, "Отложенный старт".
[[nodiscard]] bool has_engine() noexcept;

// A channel group. Looked up first by file name (the engine's own GetChannelGroup(const char*)),
// then - if that misses - by pool name across every loaded group, which is how "StatCollector"
// resolves. Null when the group is not loaded.
[[nodiscard]] A3d_ChannelGroup* find_group(const char* name) noexcept;

// How many channel groups the engine currently has loaded, and a describing string for one of them
// ("<pool name> | <file name>"). Purely a diagnostic: when a script cannot find a group, the useful
// answer is "here is what is actually loaded right now", and group membership changes as the game
// moves between menu and run.
//
// The returned pointer is module-owned and valid until the next call.
[[nodiscard]] int group_count() noexcept;
[[nodiscard]] const char* group_describe(int index) noexcept;

// A channel by name inside a group. Null when either is missing. Says nothing about its type.
[[nodiscard]] A3d_Channel* find_channel(A3d_ChannelGroup* group, const char* name) noexcept;

// A channel by its index inside the group.
//
// Needed because **channel names are not unique within a group**: TrafficCommander has two Values
// called "TrafficType" (#631 and #900), and GetChannel(const char*) returns whichever comes first in
// scan order - which is not the one Do_CollectCar writes. When a name is ambiguous the index is the
// only way to say which channel is meant.
[[nodiscard]] A3d_Channel* find_channel_at(A3d_ChannelGroup* group, int index) noexcept;

// What family this channel belongs to, from its ChannelType record. Cheap after the first call per
// channel: the engine caches the record on the object itself (channelTypeP_, +0x0c).
[[nodiscard]] kind kind_of(A3d_Channel* channel) noexcept;

// Whether the vtable slot a given kind's accessor would call points at real code in a mapped module.
// A last line of defence under kind_of(): a correctly typed channel is still only worth calling
// through if its vtable looks sane.
[[nodiscard]] bool is_callable_as(A3d_Channel* channel, kind as) noexcept;

// Evaluates a numeric channel through vtable slot 17 (+0x44). Callers must have established the kind
// first - this does not re-check, because it is the hot path. Returns 0 for a null channel.
[[nodiscard]] float get_float(A3d_Channel* channel) noexcept;

// Writes a numeric channel through vtable slot 19 (+0x4c). Same contract as get_float: the kind must
// already be known good. On an Aco_FloatChannel this is a plain store with no side effects.
void set_float(A3d_Channel* channel, float value) noexcept;

// Writes a vector channel through vtable slot 18 (+0x48), `Aco_VectorChannel::SetVector`.
//
// **The kind must already be known good, and here that is a hard safety requirement rather than
// tidiness.** Slot 19 is `SetFloat(float)` on a numeric channel and `SetFloat(int, float)` on a
// vector one - calling the wrong one is a stack mismatch, not a wrong value (engine journal §2.2.2).
// What makes that impossible in practice is the typed resolve: a handle only reaches here after
// kind_of() reported `vector`, i.e. after the channel's baseguid was read off its own ChannelType.
// That is the whole protection, and it is why there is no accessor that takes an unverified channel.
//
// The three floats are passed exactly as the engine's own callers pass them. SetVector takes
// D3DXVECTOR3 **by value**, and the disassembly shows it reading three consecutive dwords from
// [esp+4] - byte-identical to three float arguments, so the thunk needs no struct type of its own.
// (Verified against the shipped binary, not inferred from the ABI rules; see the journal.)
//
// **This is not a local store.** SetVector writes x/y/z into the channel and then walks children
// 0, 1 and 2: for each one whose baseguid is the *numeric* family it calls `child->SetFloat(component)`.
// So writing a `Value Vector` also writes through into whatever Value channels feed its components.
// Harmless where those are plain values, invisible where they are computed (the next evaluation
// overwrites), but it is a wider blast radius than set_float and callers should know it.
void set_vector(A3d_Channel* channel, float x, float y, float z) noexcept;

// Evaluates a vector channel through vtable slot 17 (+0x44), writing x/y/z into `out`. Same contract
// as get_float: the kind must already be known good.
//
// Slot 17 is GetFloat on a numeric channel and GetVector here - the collision is the whole reason
// kind_of() gates every accessor. Worse, slot 19 is SetFloat on both families with *different
// signatures*: `SetFloat(float)` numerically, `SetFloat(int, float)` on a vector. Writing to a vector
// channel through the numeric setter is a stack imbalance, not a wrong number, which is why there is
// no vector write here at all.
//
// The engine returns D3DXVECTOR3 by value, so on x86 MSVC this is a hidden-buffer call exactly like
// GetChannelType (reversing-journal-engine.md §3.2). False on a null channel or an unusable vtable,
// with `out` left alone.
bool get_vector(A3d_Channel* channel, float out[3]) noexcept;

// Reads one cell of an Array Table through its cursor pair, which is how the engine addresses table
// columns: an `Array Value` channel takes its row index from the separate channel wired to its port
// 0 (reversing-journal-engine.md §5).
//
// Does the whole save/set/read/restore dance, and the restore is the part that matters - the cursor
// is shared with the game, and leaving it moved corrupts whatever the graph does next, not just what
// we read. Also busts the per-frame memo when (and only when) the array channel is one of the rare
// ones that has it enabled, so reading several indices in one frame returns several values.
[[nodiscard]] float read_array(A3d_Channel* array_value, A3d_Channel* indexer, float index) noexcept;

// Same, for an `Array Vector` column - the cursor mechanism is identical, only the accessor differs
// (slot 17 is GetVector here, not GetFloat).
//
// Needed for `StatCollector::Stats: TrafficPattern`, the generated track laid out one row per block
// with the colour id in x. That table is the game's own source of truth for how much traffic of each
// colour a track contains - `Do_GetTrafficCounts` walks exactly this - and unlike the summary
// columns it is complete from the moment the track is generated.
[[nodiscard]] bool read_array_vector(A3d_Channel* array_vector, A3d_Channel* indexer, float index, float out[3]) noexcept;

// How many rows the table behind an `Array Value` / `Array Vector` column currently has, or -1 when
// the channel is not one of those two types or its table is not connected.
//
// This is not a convenience: it is the number that makes writing a row safe at all. See write_array.
[[nodiscard]] int array_row_count(A3d_Channel* column) noexcept;

// Whether `row` exists in that column's table *without creating it*. The distinction matters - the
// engine's own row accessor comes in two flavours and the creating one is what its setter path uses.
[[nodiscard]] bool array_has_row(A3d_Channel* column, int row) noexcept;

// Writes one cell of an Array Table, the mirror of read_array / read_array_vector: same cursor pair,
// same save/set/act/restore, and the restore is equally unconditional.
//
// **The bounds check is the reason these exist as their own functions rather than as "set the cursor
// and call set_float".** The engine does not validate the row index anywhere on this path:
// `ArrayConnectItem::SetData` asks its column for the row, and *when the row is missing it creates
// it* (engine journal §2.2.3). So an index past the end is not a no-op and not a wrong-cell write -
// it appends to the game's table, changing what every other reader of that table sees. Refusing an
// absent row is therefore part of the operation, not a nicety, and it is done with the engine's own
// non-creating accessor rather than by comparing against a row count we might have read stale.
//
// Two things are deliberately different from the read path:
//
// - **No memo bust before the write.** The setters never call CheckRenderCount, so a write always
//   lands; the invalidation the readers need has no counterpart here.
// - **A memo bust *after* it.** Both setters also store the value into the channel's own scalar field
//   (+0x7c) as a side effect, without touching the memo. If the channel is one of the memoised ones
//   and the game had already evaluated it this frame, its next read in the same frame would return
//   that scalar - our value - no matter which row its cursor points at. Invalidating afterwards
//   forces the next reader back to the table.
//
// False means the write did not happen: unresolved channel, wrong type, disconnected table, or a row
// that does not exist. True means the setter was called with the cursor on the requested row - which
// is as much as can be promised, because the engine's setters return void and can still skip the
// table silently when it is not connected (see array_row_count, which refuses in that same case).
bool write_array(A3d_Channel* array_value, A3d_Channel* indexer, float index, float value) noexcept;

// Same for an `Array Vector` column. Note that the value goes in through slot 18 (`SetVector`),
// which on this type is the table write - not the component-propagating store that a plain
// `Value Vector` performs.
bool write_array_vector(A3d_Channel* array_vector, A3d_Channel* indexer, float index, float x, float y, float z) noexcept;

// Reads a text channel. Handles both storage modes: the channel is asked whether it holds wide
// characters (slot 23) and the wide string (slot 24) is converted to UTF-8 when it does - which is
// what song titles actually need.
//
// The returned pointer is owned by this module and is valid only until the next get_text() call.
// Never null; an unreadable channel yields "".
[[nodiscard]] const char* get_text(A3d_Channel* channel) noexcept;

// The channel's name field, read straight out of +0x50 through the exported one-instruction
// accessor. Null-safe; may return null.
[[nodiscard]] const char* channel_name(A3d_Channel* channel) noexcept;
} // namespace tw::lua::channels
