#include "pch.hxx"

#include "engine/family/fam_text.hxx"

#include "engine/channel_vtable.hxx"

namespace
{
// Aco_StringChannel, from its own vtable dump. See the header for what slot 17 is on everyone else.
constexpr std::size_t k_get_string = 0x44;
constexpr std::size_t k_get_if_use_wchar = 0x5c;
constexpr std::size_t k_get_wstring = 0x60;

using get_string_fn = const char*(__fastcall*)(A3d_Channel*, void*);
using get_wstring_fn = const wchar_t*(__fastcall*)(A3d_Channel*, void*);
using get_bool_fn = bool(__fastcall*)(A3d_Channel*, void*);

std::string g_buffer;
} // namespace

namespace tw::engine::fam_text
{
bool callable(A3d_Channel* channel) noexcept
{
    return vtable::slot_is_code(channel, k_get_string) && vtable::slot_is_code(channel, k_get_if_use_wchar)
        && vtable::slot_is_code(channel, k_get_wstring);
}

const char* get(const channel_ref& ref) noexcept
{
    g_buffer.clear();

    if(ref.family != kind::text || ref.channel == nullptr) [[unlikely]] {
        return g_buffer.c_str();
    }

    if(vtable::slot<get_bool_fn>(ref.channel, k_get_if_use_wchar)(ref.channel, nullptr)) {
        const wchar_t* wide = vtable::slot<get_wstring_fn>(ref.channel, k_get_wstring)(ref.channel, nullptr);
        if(wide == nullptr || *wide == L'\0') {
            return g_buffer.c_str();
        }

        // UTF-8 on the way out: this ends up in Lua strings and in ImGui, both of which expect it.
        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        if(needed > 1) {
            g_buffer.resize(static_cast<std::size_t>(needed) - 1);
            ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, g_buffer.data(), needed, nullptr, nullptr);
        }

        return g_buffer.c_str();
    }

    const char* narrow = vtable::slot<get_string_fn>(ref.channel, k_get_string)(ref.channel, nullptr);
    if(narrow != nullptr) {
        g_buffer.assign(narrow);
    }

    return g_buffer.c_str();
}
} // namespace tw::engine::fam_text
