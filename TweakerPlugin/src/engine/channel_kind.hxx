#pragma once

// Which family a channel belongs to - the one question that has to be answered before any
// family-specific slot is called, and the one place that knows about every family at once.
//
// **Why the answer comes from the channel's ChannelType and not from anything else.** Slots 17-19
// mean different things on every family, and on some families calling the wrong one is not a wrong
// number but a draw call, a Release() or a stack mismatch (reversing-journal-boot.md §7.1). The
// ChannelType record carries a base guid that partitions all 226 channel types into families
// (engine journal §3.2); reading it is the only classification that does not rest on a guess.
//
// Cold path: this runs once per resolve and never per frame. The family it decides is stored in the
// channel_ref, and every per-frame call after that trusts the ref.
namespace tw::engine
{
// The channel families the engine's type system distinguishes. **The numeric values are part of the
// scripting ABI** - the Lua prelude passes them as integers to the resolve entry points - so they
// are appended to, never renumbered.
enum class kind : int {
    unknown = -1, // could not be determined - no type record, or the object is not usable
    number = 0,   // Aco_FloatChannel: Value, Expression Value, Trigger, Array Value, Lua Script, ...
    text = 1,     // Aco_StringChannel: Text, Array Text, TextOperator, ...
    vector = 2,   // Aco_VectorChannel: Value Vector, Array Vector, ...
    matrix = 3,   // Aco_MatrixChannel: Matrix, Array Matrix, MatrixMotion, ...
    texture = 4,  // Aco_DX8_Texture - no accessor, and slot 19 is Release()
    object = 5,   // Aco_DX8_ObjectDataChannel - no accessor, and slot 19 is Release()
    other = 6,    // a known channel of a family nothing here has an accessor for
};
} // namespace tw::engine

namespace tw::engine::channel_kind
{
// Size of a ChannelType record, verified against the binary: A3d_Channel::GetChannelType does
// operator_new(0x84) and copies 0x21 dwords into it.
constexpr std::size_t k_type_record_size = 0x84;

// "float", "text", "vector", "matrix", "texture", "object", "other", "unknown". For error messages
// that name both the family a script asked for and the one it got.
[[nodiscard]] const char* name(kind value) noexcept;

// The family, from the channel's base guid. kind::unknown for null or for an object whose type
// record cannot be read.
[[nodiscard]] kind of(A3d_Channel* channel) noexcept;

// Whether the slot that family's reader would call points at code in this object's vtable. A last
// line of defence under of(): a correctly typed channel is still only worth calling through if its
// vtable looks sane. Always false for families with no accessor.
[[nodiscard]] bool callable_as(A3d_Channel* channel, kind as) noexcept;

// Whether the channel's own type guid - not the family's base guid - is exactly `guid`. For the
// handful of places that need one specific type: Array Value and Array Vector keep their table
// connection at offsets that exist on neither of their parents, so "is a number" is not enough to
// read it.
[[nodiscard]] bool is_type(A3d_Channel* channel, const GUID& guid) noexcept;
} // namespace tw::engine::channel_kind
