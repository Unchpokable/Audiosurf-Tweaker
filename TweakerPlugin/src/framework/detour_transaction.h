#pragma once

#include <Windows.h>
#include <initializer_list>

// Thin wrapper around Microsoft Detours transactions. Detours requires every
// DetourAttach/DetourDetach to happen inside an explicit
// Begin/UpdateThread/Commit transaction (unlike kiero::bind, which hid all of
// that). This exposes it as one batched call instead of one transaction per
// hook.
namespace tw::framework::detour
{
struct binding
{
    void** target;
    void* replacement;
};

// Attaches every binding in one transaction: a single process-wide thread
// suspend/resume instead of one per hook, and all-or-nothing (if any
// DetourAttach in the batch fails, the whole batch is rolled back).
bool attach(std::initializer_list<binding> bindings);

// Same batching as attach(), for DetourDetach.
bool detach(std::initializer_list<binding> bindings);
} // namespace tw::framework::detour
