#pragma once

namespace utils
{
constexpr std::string_view offset_view(const void* ptr)
{
    return std::format("0x{:X}", reinterpret_cast<std::uintptr_t>(ptr));
}
}