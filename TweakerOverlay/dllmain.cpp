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
    }
    return TRUE;
}
