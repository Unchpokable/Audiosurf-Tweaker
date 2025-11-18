#include "global.h"

#include "proc_tools.hxx"

#define NOMINMAX
#include <Windows.h>

#include <TlHelp32.h>

std::uint64_t mmap::tools::get_proc_id(std::wstring_view proc_name)
{
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if(snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 proc_entry;

    proc_entry.dwSize = sizeof(PROCESSENTRY32);

    if(!Process32First(snapshot, &proc_entry)) {
        CloseHandle(snapshot);
        return 0;
    }

    do {
        std::wstring_view proc_name_view(proc_entry.szExeFile, std::wcslen(proc_entry.szExeFile));
        if(proc_name_view.find(proc_name) != std::wstring_view::npos) {
            CloseHandle(snapshot);
            return static_cast<std::uint64_t>(proc_entry.th32ProcessID);
        }
    } while(Process32Next(snapshot, &proc_entry));

    CloseHandle(snapshot);

    return 0;
}
