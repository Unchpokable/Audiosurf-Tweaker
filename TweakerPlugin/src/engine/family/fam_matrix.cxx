#include "pch.hxx"

#include "engine/family/fam_matrix.hxx"

#include "engine/channel_vtable.hxx"

namespace
{
// Aco_MatrixChannel slots 17 and 19, from ??_7Aco_MatrixChannel@@6B@ at RVA 0x20e8. Slot 18 is the
// same function as 17 and is not needed.
constexpr std::size_t k_get_matrix = 0x44;
constexpr std::size_t k_set_matrix = 0x4c;

// D3DXMATRIX without d3dx9: sixteen floats, trivially copyable, no bases. Passed by value it is
// copied onto the stack as sixteen dwords, which is exactly what SetMatrix reads (`lea esi,[esp+0xc]`
// after two pushes, then 16 x movsd) and exactly what its `ret 0x40` pops.
struct matrix_value {
    float m[tw::engine::fam_matrix::k_elements];
};

static_assert(sizeof(matrix_value) == 64, "D3DXMATRIX is 64 bytes, and SetMatrix pops exactly that many");
static_assert(std::is_trivially_copyable_v<matrix_value>, "must be passed by value as raw dwords");

using get_matrix_fn = void*(__fastcall*)(A3d_Channel*, void*, matrix_value*);
using set_matrix_fn = void(__fastcall*)(A3d_Channel*, void*, matrix_value);
} // namespace

namespace tw::engine::fam_matrix
{
bool callable(A3d_Channel* channel) noexcept
{
    return vtable::slot_is_code(channel, k_get_matrix) && vtable::slot_is_code(channel, k_set_matrix);
}

bool get(const channel_ref& ref, float out[k_elements]) noexcept
{
    if(ref.family != kind::matrix || ref.channel == nullptr || out == nullptr) [[unlikely]] {
        return false;
    }

    alignas(4) matrix_value buffer {};
    vtable::slot<get_matrix_fn>(ref.channel, k_get_matrix)(ref.channel, nullptr, &buffer);

    std::memcpy(out, buffer.m, sizeof(buffer.m));

    return true;
}

void set(const channel_ref& ref, const float in[k_elements]) noexcept
{
    if(ref.family != kind::matrix || ref.channel == nullptr || in == nullptr) [[unlikely]] {
        return;
    }

    matrix_value value {};
    std::memcpy(value.m, in, sizeof(value.m));

    vtable::slot<set_matrix_fn>(ref.channel, k_set_matrix)(ref.channel, nullptr, value);
}
} // namespace tw::engine::fam_matrix
