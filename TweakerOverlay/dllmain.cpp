#include "pch.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "plugin.hpp"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch(reason) {
        case DLL_PROCESS_ATTACH: {
            tw::plugin::load(nullptr);
            return TRUE;
        }
        case DLL_PROCESS_DETACH: {
            tw::plugin::unload(nullptr);
            return TRUE;
        }
        default: ;
    }
    return TRUE;
}
