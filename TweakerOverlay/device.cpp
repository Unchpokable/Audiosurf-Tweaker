#include "pch.hpp"

#include "device.hpp"
#include "native_hooks.hpp"

IDirect3DDevice9* tw::native::current_device = nullptr;

namespace
{
std::unordered_map<tw::native::DxFunction, void*> active_hooks;
}

void tw::native::change_device(IDirect3DDevice9* device)
{
    for(auto hook : active_hooks | std::views::keys) {
        remove_hook(hook);
    }

    current_device = device;

    for(auto [hook, function] : active_hooks) {
        set_hook(hook, function);
    }
}

tw::native::HookResult tw::native::set_hook(DxFunction function, void* hook)
{
    if(!hook) {
        return WrongPointer;
    }

    if(active_hooks.contains(function)) {
        return AlreadyInUse;
    }

    auto vtable = *reinterpret_cast<void***>(current_device);

    auto vptr = vtable[static_cast<int>(function)];

    auto result = detour_attach_hook(&static_cast<PVOID&>(vptr), hook);

    if(result != NO_ERROR) {
        return GenericFailure;
    }

    return Success;
}

tw::native::HookResult tw::native::remove_hook(DxFunction function)
{
    if(!active_hooks.contains(function)) {
        return NotFound;
    }

    auto vtable = *reinterpret_cast<void***>(current_device);

    auto vptr = vtable[static_cast<int>(function)];

    auto hook = active_hooks.at(function);

    auto result = detour_detach_hook(&static_cast<PVOID&>(vptr), hook);

    if(!result != NO_ERROR) {
        return GenericFailure;
    }

    return Success;
}