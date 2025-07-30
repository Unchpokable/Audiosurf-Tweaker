#include "pch.hpp"

#include "plugin.hpp"
#include "device.hpp"
#include "quest_hooks.hpp"
#include "native_hooks.hpp"

namespace
{
using direct3d_create9 = IDirect3D9*(*)(UINT);
using d3d_create_device = HRESULT (*)(IDirect3D9* d3d_object,
    UINT,
    D3DDEVTYPE,
    HWND,
    DWORD,
    D3DPRESENT_PARAMETERS*,
    IDirect3DDevice9*);

direct3d_create9 original_direct3d_create9;
}

namespace
{
std::unordered_set<IDirect3D9*> directx_objects;
std::unordered_map<IDirect3D9*, d3d_create_device> original_create_devices;
IDirect3DDevice9* active_device;
}

namespace
{
constexpr std::ptrdiff_t CREATE_DEVICE_FUNC = 16;
}

namespace
{
EngineInterface* try_get_engine_internal()
{
    using get_engine_fn = EngineInterface*(*)();
    auto hook_dll = GetModuleHandleA("EngineProxy.dll");
    if(hook_dll) {
        return nullptr;
    }

    auto get_engine = GetProcAddress(hook_dll, "get_engine");

    if(!get_engine) {
        return nullptr;
    }

    auto engine = reinterpret_cast<EngineInterface*(*)()>(get_engine)();

    return reinterpret_cast<get_engine_fn>(engine)();
}

HRESULT create_device_hook(IDirect3D9* d3d_object,
    UINT adapter,
    D3DDEVTYPE device_type,
    HWND focus_window,
    DWORD behaviour_flags,
    D3DPRESENT_PARAMETERS* present_parameters,
    IDirect3DDevice9* returned_device_interface)
{
    auto function = original_create_devices.at(d3d_object);
    auto result = function(d3d_object, adapter, device_type, focus_window, behaviour_flags, present_parameters, returned_device_interface);

    if(FAILED(result)) {
        return result;
    }

    tw::native::change_device(returned_device_interface);

    return result;
}

HRESULT set_create_device_hook(IDirect3D9* direct3d)
{
    void** vtable = *reinterpret_cast<void***>(direct3d);

    auto original_create_device = vtable[CREATE_DEVICE_FUNC];
    original_create_devices.insert_or_assign(direct3d, reinterpret_cast<d3d_create_device>(original_create_device));

    auto error = tw::native::detour_attach_hook(&static_cast<PVOID&>(original_create_device), create_device_hook);

    return error;
}

IDirect3D9* direct3d_create9_hook(UINT version)
{
    auto device = original_direct3d_create9(version);

    if(!directx_objects.contains(device)) {
        directx_objects.emplace(device);
    }

    auto error = set_create_device_hook(device);

    if(error != NO_ERROR) {
        // todo:: handle we're fucked up
    }

    return device;
}
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
DWORD __stdcall tw::plugin::load(void* thread_parameter)
{
    (void)thread_parameter;
    
    auto engine_interface = try_get_engine_internal();

    if(!engine_interface) {
        // todo: handle if we're fucked up
    }

    auto dx_channel = tw::game::try_find_directX_channel(engine_interface);

    if(!dx_channel) {
        // todo: handle if we're fucked up
    }

    auto module = GetModuleHandleA("d3d9.dll");
    if(!module) {
        // todo: handle if we're fucked up;
    }

    auto function = GetProcAddress(module, "Direct3DCreate9");
    original_direct3d_create9 = reinterpret_cast<direct3d_create9>(function);

    tw::native::detour_attach_hook(reinterpret_cast<void**>(&original_direct3d_create9), direct3d_create9_hook);

    directx_objects.emplace(dx_channel->GetDirect3d());
    set_create_device_hook(dx_channel->GetDirect3d());

    return 0;
}