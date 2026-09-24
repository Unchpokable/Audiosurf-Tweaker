#include "pch.hxx"

#include "engine/channel_vtable.hxx"

namespace tw::engine::vtable
{
bool is_code(const void* address) noexcept
{
    if(address == nullptr) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info {};
    if(::VirtualQuery(address, &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT) {
        return false;
    }

    constexpr DWORD k_executable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

    return (info.Protect & k_executable) != 0;
}

bool slot_is_code(const void* object, std::size_t byte_offset) noexcept
{
    if(object == nullptr) {
        return false;
    }

    const void* const* table = of(object);
    if(table == nullptr) {
        return false;
    }

    return is_code(table[byte_offset / sizeof(void*)]);
}
} // namespace tw::engine::vtable
