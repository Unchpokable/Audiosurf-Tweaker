#include "global.h"

#include "context32.h"
#include "mmap32.hxx"

#include "win_handle.hxx"

namespace
{
// todo: compile shl32.masm and replace it
const std::uint8_t shellcode_blob_32[] = {
    0x60,
    0x8B,
    0xF0,
    0x56,
    0x8B,
    0x1E,
    0xFF,
    0xD3,
    0x61,
    0x8B,
    0x46,
    0x08,
    0xFF,
    0xE0,
};
} // namespace

mmap::MmapResult mmap::mmap_load_dll(std::string_view dll_path, std::uint64_t proc_id) noexcept
{
    using NtSuspendProcess = NTSTATUS(NTAPI*)(HANDLE);
    using NtResumeProcess = NTSTATUS(NTAPI*)(HANDLE);

    auto nt_suspend = reinterpret_cast<NtSuspendProcess>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSuspendProcess"));
    auto nt_resume = reinterpret_cast<NtResumeProcess>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtResumeProcess"));

    auto proc_handle = raii::WinHandle::create<&OpenProcess>(PROCESS_ALL_ACCESS, 0, proc_id);

    nt_suspend(proc_handle);

    // do stuff

    nt_resume(proc_handle);

    return MMAP_OK;
}
