#pragma once

#include <DbgHelp.h>

namespace sym
{
enum SymbolStatus {
    Success,
    CouldNotLoadCom,
    CouldNotLoadModule,
    NoTargetModule,
};
}

namespace sym
{
SymbolStatus initialize();
}

namespace sym
{
std::uintptr_t find_symbol_address(const char* symbol_name);
}
