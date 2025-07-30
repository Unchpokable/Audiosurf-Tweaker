#pragma once

namespace tw::native
{
}

namespace tw::native
{
HRESULT __stdcall detour_attach_hook(void** pointer, void* detour);
HRESULT __stdcall detour_detach_hook(void** pointer, void* detour);
}
