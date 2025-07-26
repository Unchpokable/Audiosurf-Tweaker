#pragma once

namespace tw::plugin
{
DWORD __stdcall load(void* thread_parameter);
DWORD __stdcall unload(void* thread_parameter);
}