#include "global.h"

#include "proc_tools.hxx"

#include "win_handle.hxx"

std::uint64_t mmap::tools::get_proc_id(std::string_view proc_name) noexcept
{
    auto snapshot = raii::WinHandle::create<&CreateToolhelp32Snapshot>(TH32CS_SNAPPROCESS, 0);

    if(snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 proc_entry;

    proc_entry.dwSize = sizeof(PROCESSENTRY32);

    if(!Process32First(snapshot, &proc_entry)) {
        return 0;
    }

    do {
        std::string_view proc_name_view(proc_entry.szExeFile, std::strlen(proc_entry.szExeFile));
        if(proc_name_view.find(proc_name) != std::string_view::npos) {
            return static_cast<std::uint64_t>(proc_entry.th32ProcessID);
        }
    } while(Process32Next(snapshot, &proc_entry));

    return 0;
}

mmap::memory::StaticString<MAX_PATH> mmap::tools::get_proc_root(std::string_view proc_name) noexcept
{
    auto proc_id = get_proc_id(proc_name);

    auto proc = raii::WinHandle::create<&OpenProcess>(PROCESS_QUERY_LIMITED_INFORMATION, 0, proc_id);

    mmap::memory::StaticString<MAX_PATH> path;

    DWORD size = MAX_PATH;

    bool success = QueryFullProcessImageNameA(proc, 0, path.data(), &size);

    return { std::filesystem::path(path.c_str()).parent_path().string().c_str() };
}
