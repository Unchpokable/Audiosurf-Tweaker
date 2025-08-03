#pragma once

namespace tw::plugin
{
unsigned __stdcall load(void* thread_parameter);
unsigned __stdcall unload(void* thread_parameter);
}