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
