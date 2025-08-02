#include "pch.hpp"

#include "plugin.hpp"
#include "device.hpp"
#include "native_hooks.hpp"


namespace
{
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
DWORD __stdcall tw::plugin::load(void* thread_parameter)
{
    (void)thread_parameter;

    return 0;
}