#pragma once

#include "engine/channel_ref.hxx"

// Aco_VectorChannel - the vector family. Base guid VECTOR_GUID (9D045960-...), "Value Vector" and
// everything derived from it, Array Vector included. The table ends at slot 20 - past it in .rdata is
// the string "Value Vector" - so a vector channel has exactly three slots of its own:
//
//   slot 17  +0x44  GetVector() -> D3DXVECTOR3   BY VALUE: a hidden result pointer is the first
//                                                stack argument, same shape as GetChannelType
//   slot 18  +0x48  SetVector(D3DXVECTOR3)       BY VALUE: three dwords on the stack
//   slot 19  +0x4c  SetFloat(int, float)         <-- NOT SetFloat(float) as on the numeric family
//
// WHAT A WRONG FAMILY COSTS. Slot 19 is occupied on both this family and the numeric one with
// different signatures: the numeric setter called on a vector channel pops one argument fewer than
// its caller pushed. That is a stack mismatch, not a wrong value (engine journal §2.2.2). And slot 17
// is GetFloat there: calling it with our hidden-result convention on a numeric channel returns a
// float in st(0) and leaves our buffer - and the stack - to fend for themselves.
//
// **SetVector is not a local store.** It writes x/y/z into the channel and then walks children 0-2:
// for each one of the numeric family it calls child->SetFloat(component). Writing a Value Vector also
// writes into whatever Value channels feed its components - harmless where those are plain values,
// undone on the next evaluation where they are computed, and a wider reach than fam_number::set.
namespace tw::engine::fam_vector
{
[[nodiscard]] bool callable(A3d_Channel* channel) noexcept;

// x, y, z into `out`. False for a ref of another family, with `out` untouched.
bool get(const channel_ref& ref, float out[3]) noexcept;

// On an Array Vector this is the table write - Aco_Array_Vector overrides slot 18 - rather than the
// component-propagating store of a plain Value Vector. See fam_table.
void set(const channel_ref& ref, float x, float y, float z) noexcept;
} // namespace tw::engine::fam_vector
