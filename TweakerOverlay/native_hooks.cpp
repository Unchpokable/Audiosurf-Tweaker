#include "pch.hpp"

#include "native_hooks.hpp"

HRESULT __stdcall tw::native::detour_attach_hook(void** pointer, void* detour)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    auto result = DetourAttach(pointer, detour);

    DetourTransactionCommit();
    return result;
}

HRESULT __stdcall tw::native::detour_detach_hook(void** pointer, void* detour)
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    auto result = DetourDetach(pointer, detour);

    DetourTransactionCommit();
    return result;
}