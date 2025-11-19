#include "global.h"

#include "proc_check.hxx"

#include "static_string.hxx"
#include "win_handle.hxx"

namespace
{
std::vector<mmap::memory::StaticString<MAX_PATH>> enum_proc_dlls(std::uint64_t proc_id) noexcept
{
    auto process = mmap::raii::WinHandle::create<&OpenProcess>(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, 0, proc_id);

    std::array<HMODULE, 1024> modules;

    DWORD bytes_needed;

    if(EnumProcessModules(process, modules.data(), modules.size(), &bytes_needed)) {
        auto module_count = bytes_needed / sizeof(HMODULE);

        std::vector<mmap::memory::StaticString<MAX_PATH>> module_names;
        module_names.reserve(module_count);

        for(std::size_t i { 0 }; i < module_count; ++i) {
            auto module = modules[i];
            mmap::memory::StaticString<MAX_PATH> module_name;

            if(GetModuleFileNameExA(process, module, module_name.data(), MAX_PATH)) {
                module_names.emplace_back(std::move(module_name));
            }
        }
    }

    return {};
}
} // namespace

bool mmap::proc_has_external_deps(std::uint64_t proc_id, const std::vector<mmap::memory::StaticString<MAX_PATH>>& dependent_dlls) noexcept
{
    auto dlls = enum_proc_dlls(proc_id);

    return std::ranges::all_of(dependent_dlls, [&](const auto& elem) {
        return std::ranges::find(dlls, elem) != std::ranges::end(dlls);
    });
}
