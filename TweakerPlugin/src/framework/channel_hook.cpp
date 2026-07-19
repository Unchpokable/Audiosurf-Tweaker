#include "pch.h"

#include "hooks/channel_hook.h"

#include "quest3d/state.h"

namespace
{
void(__thiscall* true_call_channel)(A3d_Channel* self) = nullptr;

void __fastcall call_channel_hook(A3d_Channel* self, DWORD /*edx*/)
{
    true_call_channel(self);
    if(quest3d::g_engine == nullptr) {
        quest3d::g_engine = self->engine;
    }
}
} // namespace

namespace hooks
{
void install_channel_hook()
{
    true_call_channel = (void(__thiscall*)(A3d_Channel*))DetourFindFunction("highpoly.dll", "?CallChannel@A3d_Channel@@UAEXXZ");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)true_call_channel, &call_channel_hook);
    DetourTransactionCommit();
}
} // namespace hooks
