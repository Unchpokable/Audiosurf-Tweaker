#include "pch.hxx"

#include "plugin/load.hxx"

namespace
{
unsigned __stdcall load_thread_entry(void* parameter)
{
    tw::plugin::load_thread(static_cast<HMODULE>(parameter));
    return 0;
}
} // namespace

BOOL APIENTRY DllMain(HMODULE h_module, DWORD dw_reason, LPVOID /*lp_reserved*/)
{
    switch(dw_reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(h_module);
            _beginthreadex(nullptr, 0, load_thread_entry, h_module, 0, nullptr);
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
