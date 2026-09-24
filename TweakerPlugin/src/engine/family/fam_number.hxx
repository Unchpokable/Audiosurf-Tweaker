#pragma once

#include "engine/channel_ref.hxx"

// Aco_FloatChannel - the numeric family. Base guid FLOAT_CHANNEL_GUID; 36 channel types derive from
// it, among them Value, Expression Value, Trigger, Array Value and Lua Script. Everything numeric in
// the game is one of these, flags and enumerations included.
//
//   slot 17  +0x44  GetFloat() -> float        memoised through CheckRenderCount
//   slot 18  +0x48  GetOldFloat() -> float     not used here
//   slot 19  +0x4c  SetFloat(float)
//
// WHAT A WRONG FAMILY COSTS (reversing-journal-boot.md §7.1). These are the two slots the rest of the
// engine collides with hardest:
//
//   GetFloat on a Text channel      -> GetString: a pointer read back as a float. The mild case.
//   GetFloat on a Texture / 3D Data -> InvalidateDeviceObjects: the game loses a device resource.
//   GetFloat on a 3D Object         -> DrawSurfaces: the object is drawn in the middle of our call.
//   SetFloat on a Value Vector      -> SetFloat(int, float): one argument short, a stack mismatch.
//   SetFloat on a Texture / 3D Data -> Release(), with an extra argument on the stack.
//   SetFloat on a 3D Object         -> GetObjectMatrix(D3DXMATRIX*): our float taken as a pointer
//                                      and 64 bytes written through it.
//
// None of those looks like an error at the moment it happens. So nothing here accepts an unchecked
// channel: the entry points take a channel_ref, whose family was read off the channel's own
// ChannelType at resolve, and they refuse a ref of any other family at the cost of one compare.
namespace tw::engine::fam_number
{
// Whether this family's reader slot is code in this object's vtable. Resolve time only.
[[nodiscard]] bool callable(A3d_Channel* channel) noexcept;

// Evaluates the channel through its **own** vtable - not by calling the exported
// Aco_FloatChannel::GetFloat with an explicit `this`, which would run the base implementation on
// every channel and hand back an Expression Value's child instead of its formula (engine journal
// §2.5). 0 for a ref of another family.
[[nodiscard]] float get(const channel_ref& ref) noexcept;

// On a plain Aco_FloatChannel a store with no side effects. On a derived type it is whatever that
// type's setter does - Aco_Array_Value::SetFloat writes a table row, for one (see fam_table).
void set(const channel_ref& ref, float value) noexcept;
} // namespace tw::engine::fam_number
