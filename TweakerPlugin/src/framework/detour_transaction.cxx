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

    THREADENTRY32 entry {};
    entry.dwSize = sizeof(entry);

    const DWORD this_process = GetCurrentProcessId();
    const DWORD this_thread = GetCurrentThreadId();

    if(Thread32First(snapshot, &entry)) {
        do {
            if(entry.dwSize < FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(entry.th32OwnerProcessID)) {
                continue;
            }
            if(entry.th32OwnerProcessID != this_process || entry.th32ThreadID == this_thread) {
                continue;
            }

            // DetourUpdateThread() suspends `thread` immediately and keeps this exact handle to
            // resume it later in DetourTransactionCommit()/Abort(). Closing it here would race
            // against that: the thread stays suspended (SuspendThread already ran) while the
            // handle value could get closed or even recycled by the OS before Commit's
            // ResumeThread() runs on it - a permanently-hung thread, not a mere leak. Detours
            // never closes these handles itself, so leaking one HANDLE per other thread per
            // transaction (attach/detach only happen a handful of times per process) is the
            // correct, deliberate trade-off here - matching Quest3DTamperer's reference hook.
            HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID);
            if(thread != nullptr) {
                fn(thread);
            }
        } while(Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
}

// Detours keeps one pending transaction for the whole process and refuses a second one begun on another
// thread, so two installs racing - a loader notification for the Texture channel against the game thread
// hooking its new device, or the late load's retry from a device bind against the startup thread - would
// fail one of them outright. Serialised here instead.
//
// Waiting on this from under the loader lock (the early load's notifications) is safe: whoever holds it
// is inside a transaction, and nothing a transaction does - thread snapshot, SuspendThread, VirtualProtect
// - takes the loader lock.
SRWLOCK g_transaction_lock = SRWLOCK_INIT;

bool run_transaction_locked(std::span<const tw::framework::detour::binding> bindings, bool is_attach, tw::framework::detour::suspend threads)
{
    if(DetourTransactionBegin() != NO_ERROR) {
        return false;
    }

    if(threads == tw::framework::detour::suspend::others) {
        for_each_other_thread([](HANDLE thread) {
            DetourUpdateThread(thread);
        });
    }

    bool ok = true;
    for(const auto& binding : bindings) {
        const LONG error =
            is_attach ? DetourAttach(binding.target, binding.replacement) : DetourDetach(binding.target, binding.replacement);
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

bool run_transaction(std::span<const tw::framework::detour::binding> bindings, bool is_attach, tw::framework::detour::suspend threads)
{
    ::AcquireSRWLockExclusive(&g_transaction_lock);
    const bool ok = run_transaction_locked(bindings, is_attach, threads);
    ::ReleaseSRWLockExclusive(&g_transaction_lock);

    return ok;
}

} // namespace

namespace tw::framework::detour
{

bool attach(std::initializer_list<binding> bindings, suspend threads)
{
    return run_transaction({ bindings.begin(), bindings.size() }, true, threads);
}

bool attach(std::span<const binding> bindings, suspend threads)
{
    return run_transaction(bindings, true, threads);
}

bool detach(std::initializer_list<binding> bindings)
{
    return run_transaction({ bindings.begin(), bindings.size() }, false, suspend::others);
}

} // namespace tw::framework::detour
