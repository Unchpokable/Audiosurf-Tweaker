#include "pch.hxx"

#include "engine/family/fam_number.hxx"

#include "engine/channel_vtable.hxx"

namespace
{
// Aco_FloatChannel slots 17 and 19. The meaning of these two offsets on every other family is in the
// header - it is the reason they are private to this file.
constexpr std::size_t k_get_float = 0x44;
constexpr std::size_t k_set_float = 0x4c;

using get_float_fn = float(__fastcall*)(A3d_Channel*, void*);
using set_float_fn = void(__fastcall*)(A3d_Channel*, void*, float);
} // namespace

namespace tw::engine::fam_number
{
bool callable(A3d_Channel* channel) noexcept
{
    return vtable::slot_is_code(channel, k_get_float) && vtable::slot_is_code(channel, k_set_float);
}

float get(const channel_ref& ref) noexcept
{
    if(ref.family != kind::number || ref.channel == nullptr) [[unlikely]] {
        return 0.f;
    }

    return vtable::slot<get_float_fn>(ref.channel, k_get_float)(ref.channel, nullptr);
}

void set(const channel_ref& ref, float value) noexcept
{
    if(ref.family != kind::number || ref.channel == nullptr) [[unlikely]] {
        return;
    }

    vtable::slot<set_float_fn>(ref.channel, k_set_float)(ref.channel, nullptr, value);
}
} // namespace tw::engine::fam_number
