#include "pch.hxx"

#include "engine/family/fam_vector.hxx"

#include "engine/channel_vtable.hxx"

namespace
{
// Aco_VectorChannel (channels/9D045960-...dll, ??_7Aco_VectorChannel@@6B@ at RVA 0x2130,
// cross-referenced against that DLL's exported RVAs). Slot 19 is deliberately absent - see the
// header for why this file never calls it.
constexpr std::size_t k_get_vector = 0x44;
constexpr std::size_t k_set_vector = 0x48;

// Hidden-result return: the destination is the first *stack* argument, after ecx and the dead edx.
using get_vector_fn = void*(__fastcall*)(A3d_Channel*, void*, float*);

// D3DXVECTOR3 by value. On x86 MSVC a trivially copyable 12-byte struct is pushed as three
// consecutive dwords, so three float parameters have the same stack layout. Read off the shipped
// binary rather than inferred: SetVector opens with `mov edx,[esp+8]` and addresses [esp+4] and
// [esp+0xC] individually, right where three float arguments land. harness/lua/shimtest proves the
// thunk against a real __thiscall callee.
using set_vector_fn = void(__fastcall*)(A3d_Channel*, void*, float, float, float);
} // namespace

namespace tw::engine::fam_vector
{
bool callable(A3d_Channel* channel) noexcept
{
    return vtable::slot_is_code(channel, k_get_vector) && vtable::slot_is_code(channel, k_set_vector);
}

bool get(const channel_ref& ref, float out[3]) noexcept
{
    if(ref.family != kind::vector || ref.channel == nullptr || out == nullptr) [[unlikely]] {
        return false;
    }

    // The callee writes the result, so it gets storage of its own rather than `out` reinterpreted:
    // a future caller passing something narrower cannot then be corrupted by it.
    alignas(4) float buffer[3] { 0.f, 0.f, 0.f };
    vtable::slot<get_vector_fn>(ref.channel, k_get_vector)(ref.channel, nullptr, buffer);

    out[0] = buffer[0];
    out[1] = buffer[1];
    out[2] = buffer[2];

    return true;
}

void set(const channel_ref& ref, float x, float y, float z) noexcept
{
    if(ref.family != kind::vector || ref.channel == nullptr) [[unlikely]] {
        return;
    }

    vtable::slot<set_vector_fn>(ref.channel, k_set_vector)(ref.channel, nullptr, x, y, z);
}
} // namespace tw::engine::fam_vector
