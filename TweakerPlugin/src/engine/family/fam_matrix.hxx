#pragma once

#include "engine/channel_ref.hxx"

// Aco_MatrixChannel - the matrix family, new in Ф3. Base guid MATRIX_CHANNEL_GUID (2F605354-...);
// Matrix itself plus Array Matrix, InterpolateRotationQuaternions, MatrixMotion, MatrixOperator,
// Motion, ODE Body and ProjectionMatrix. The table ends at slot 20 - past it in .rdata is the string
// "Matrix" - so, like the vector family, exactly three slots of its own:
//
//   slot 17  +0x44  GetMatrix() -> D3DXMATRIX     BY VALUE: hidden result pointer first on the stack
//   slot 18  +0x48  GetOldMatrix()                the same function (COMDAT-folded), not used here
//   slot 19  +0x4c  SetMatrix(D3DXMATRIX)         BY VALUE: all 64 bytes on the stack
//
// Read off the shipped binary (2F605354-314D-4775-86E4-1F733550B227.dll), not inferred from the ABI
// rules:
//
//   GetMatrix @ RVA 0x1190:  esi = [this+0x80]; edi = [esp+8]; rep movsd x16; ret 4
//                            - the base copies out of a D3DXMATRIX* it keeps at +0x80, into the
//                              hidden pointer, and pops exactly that one argument;
//   SetMatrix @ RVA 0x11b0:  edi = [this+0x80]; esi = &[esp+0xc]; rep movsd x16;
//                            call this->vtable[+0x28]; ret 0x40
//                            - 16 dwords taken straight off the stack, then a notification through
//                              slot 10, and the callee pops all 64 bytes.
//
// Derived types override slot 17 (MatrixMotion computes rather than copies); calling through the
// object's own vtable gets their version, which is the point of calling through the vtable at all.
//
// WHAT A WRONG FAMILY COSTS. SetMatrix pops 64 bytes. Called on any other family, slot 19 is a setter
// that pops 4 (SetFloat) or 8 (SetFloat(int, float)) or nothing (Release) - 56 to 64 bytes of stack
// left behind, which is not a wrong value but a return into garbage. The same slot on a 3D Object is
// GetObjectMatrix(D3DXMATRIX*), which would take the first dword of our matrix for a pointer and
// write 64 bytes through it.
//
// Layout: D3DXMATRIX is row-major, _11 _12 _13 _14 _21 ... _44, sixteen consecutive floats.
namespace tw::engine::fam_matrix
{
constexpr int k_elements = 16;

[[nodiscard]] bool callable(A3d_Channel* channel) noexcept;

// The 16 elements into `out`, row-major. False for a ref of another family, with `out` untouched.
bool get(const channel_ref& ref, float out[k_elements]) noexcept;

// Writes all 16 elements. Whatever the channel does with a new matrix - the base notifies through
// slot 10 - it does, exactly as when the engine sets it.
void set(const channel_ref& ref, const float in[k_elements]) noexcept;
} // namespace tw::engine::fam_matrix
