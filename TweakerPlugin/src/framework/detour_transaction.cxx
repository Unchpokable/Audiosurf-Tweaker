#include "pch.hxx"

#include "framework/detour_transaction.hxx"

namespace
{

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

            HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID);
            if(thread != nullptr) {
                fn(thread);
                CloseHandle(thread);
            }
        } while(Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

bool run_transaction(std::initializer_list<tw::framework::detour::binding> bindings, bool is_attach)
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

namespace tw::framework::detour
{

bool attach(std::initializer_list<binding> bindings)
{
    return run_transaction(bindings, true);
}

bool detach(std::initializer_list<binding> bindings)
{
    return run_transaction(bindings, false);
}

} // namespace tw::framework::detour
