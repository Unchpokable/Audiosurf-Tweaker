#include "pch.hpp"

#include "symbols_find.hpp"

#pragma comment(lib, "dbghelp.lib")

namespace
{
bool initialized { false };
}

sym::SymbolStatus sym::initialize()
{
    SymSetOptions(SYMOPT_DEBUG | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);

    if(!SymInitialize(GetCurrentProcess(), "srv*c:\\temp\\symbols*https://msdl.microsoft.com/download/symbols", TRUE)) {
        return CouldNotLoadCom;
    }

    auto module_handle = GetModuleHandleA("d3d9.dll");

    if(!module_handle) {
        return NoTargetModule;
    }

    auto baseAddress = SymLoadModuleEx(GetCurrentProcess(), NULL, "d3d9.dll", NULL, (DWORD64)module_handle, 0, NULL, 0);

    if(baseAddress == 0) {
        return CouldNotLoadModule;
    }

    initialized = true;
    return Success;
}

std::uintptr_t sym::find_symbol_address(const char* symbol_name)
{
    if(!initialized) {
        return 0;
    }

    SYMBOL_INFO* symbol = static_cast<SYMBOL_INFO*>(std::malloc(sizeof(SYMBOL_INFO) + 256));
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;

    if(SymFromName(GetCurrentProcess(), symbol_name, symbol)) {
        std::uintptr_t address = static_cast<std::uintptr_t>(symbol->Address);
        free(symbol);

        return address;
    }

    free(symbol);
    return 0;
}