#include "pch.hpp"

#include "device.hpp"

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
