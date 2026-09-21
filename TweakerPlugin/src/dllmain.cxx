#include "pch.hxx"

#include "plugin/load.hxx"

BOOL APIENTRY DllMain(HMODULE h_module, DWORD dw_reason, LPVOID /*lp_reserved*/)
{
    if(dw_reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h_module);
        tw::plugin::on_process_attach(h_module);
    }

    // Always TRUE, inert copies included: failing DLL_PROCESS_ATTACH would make the engine's channel scan -
    // or the injector - see a load error for a DLL that has simply decided to do nothing.
    return TRUE;
}
