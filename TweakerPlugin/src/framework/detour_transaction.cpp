#include "pch.h"

#include "hooks/detour_transaction.h"

namespace
{

// Detours patches function prologues in place; any thread whose instruction
// pointer is currently inside one of those prologues when the patch lands
// would crash. DetourUpdateThread() tells a pending transaction to suspend a
// given thread while it commits, but only for threads handed to it
// explicitly - this DLL can be injected while the game's own render thread
// is already running, so every other thread in the process has to be
// enumerated by hand and registered.
template<typename thread_fn>
void for_each_other_thread(thread_fn&& fn)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if(snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    const DWORD this_process = GetCurrentProcessId();
    const DWORD this_thread = GetCurrentThreadId();

    if(Thread32First(snapshot, &entry)) {
        do {
            if(entry.dwSize < FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(entry.th32OwnerProcessID))
                continue;
            if(entry.th32OwnerProcessID != this_process || entry.th32ThreadID == this_thread)
                continue;

            // A thread can legitimately have exited between the snapshot and
            // here - OpenThread failing for that reason isn't an error.
            HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID);
            if(thread != nullptr) {
                fn(thread);
            }
        } while(Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

bool run_transaction(std::initializer_list<hooks::detour::binding> bindings, bool is_attach)
{
    if(DetourTransactionBegin() != NO_ERROR) {
        return false;
    }

    for_each_other_thread([](HANDLE thread) { DetourUpdateThread(thread); });

    bool ok = true;
    for(const auto& binding : bindings) {
        const LONG error = is_attach ? DetourAttach(binding.target, binding.replacement) : DetourDetach(binding.target, binding.replacement);
        if(error != NO_ERROR) {
            ok = false;
            break;
        }
    }

    if(!ok) {
        DetourTransactionAbort();
        return false;
    }

    return DetourTransactionCommit() == NO_ERROR;
}

} // namespace

namespace hooks::detour
{

bool attach(std::initializer_list<binding> bindings)
{
    return run_transaction(bindings, true);
}

bool detach(std::initializer_list<binding> bindings)
{
    return run_transaction(bindings, false);
}

} // namespace hooks::detour
