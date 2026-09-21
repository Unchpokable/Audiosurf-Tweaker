#include "pch.hxx"

#include "framework/channel_hook.hxx"

#include "plugin/quest3d_state.hxx"

namespace
{
void(__thiscall* true_call_channel)(A3d_Channel* self) = nullptr;

// TODO: fix __fastcall, because original function is __thiscall
void __fastcall call_channel_hook(A3d_Channel* self, DWORD /*edx*/)
{
    true_call_channel(self);
    if(tw::plugin::quest3d::g_engine == nullptr) [[unlikely]] { // actually this check should be almost always false
        tw::plugin::quest3d::g_engine = self->engine;
    }
}
} // namespace

namespace tw::framework
{
bool install_channel_hook(detour::suspend threads) noexcept
{
    // GetModuleHandleW + GetProcAddress rather than DetourFindFunction, which LoadLibrary's the module
    // and falls back to loading dbghelp for symbols - neither belongs in DllMain, where the early load
    // calls this. HighPoly.dll is always mapped by then: it is the module doing the loading.
    const HMODULE highpoly = ::GetModuleHandleW(L"HighPoly.dll");
    if(highpoly == nullptr) {
        return false;
    }

    true_call_channel =
        reinterpret_cast<void(__thiscall*)(A3d_Channel*)>(::GetProcAddress(highpoly, "?CallChannel@A3d_Channel@@UAEXXZ"));

    if(!true_call_channel) {
        return false;
    }

    const bool ok = tw::framework::detour::attach(
        {
            { reinterpret_cast<void**>(&true_call_channel), reinterpret_cast<void*>(call_channel_hook) },
        },
        threads);

    if(!ok) {
        true_call_channel = nullptr;
    }

    return ok;
}
} // namespace tw::framework
