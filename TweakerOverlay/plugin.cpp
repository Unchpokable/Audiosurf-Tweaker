#include "pch.hpp"

#include "plugin.hpp"
#include "device.hpp"
#include "native_hooks.hpp"

#include "aco_direct_graphics.hpp"
#include "quest_offsets.hpp"
#include "quest_wrappers.hpp"

#include "ui.hpp"

#include <iostream>

namespace
{
constexpr GUID Aco_Direct3d_ChannelGUID = { 0xd30f7991, 0x36ac, 0x47cf, { 0x98, 0x79, 0x78, 0x17, 0x59, 0x13, 0x13, 0x88 } };
}

namespace
{
struct DllInterfaceLayout
{
    void* vptr;
    HINSTANCE dll_instance;
    void* engine_interface;
};

constexpr std::ptrdiff_t engine_field_offset = offsetof(DllInterfaceLayout, engine_interface);
}

namespace
{
std::atomic_bool initialized { false };
}

namespace
{
tw::game::AcoEngineInterface aco_engine_interface { nullptr };
tw::game::AcoChannel direct_graphics_channel { nullptr };
}

namespace
{
HRESULT(__stdcall* directx_end_scene_original)(IDirect3DDevice9* device);
}

namespace
{
HRESULT __stdcall directx_end_scene(IDirect3DDevice9* device)
{
    ui::render_ui(device);
    return directx_end_scene_original(device);
}

}

namespace
{
/// Updates the device used by the plugin.
/// @return true if the device was changed, false otherwise.
bool plugin_update_device(void* aco_graphics_channel)
{
    auto device = tw::game::graphics::get_device(aco_graphics_channel);

    if(device != tw::native::current_device) {
        tw::native::change_device(device);

        auto present_handle = tw::game::graphics::get_present_window(aco_graphics_channel);

        ui::reinit_imgui(present_handle, device);

        return true;
    }

    return false;
}

bool __fastcall aco_reset_device(void* this_)
{
    if(this_ != direct_graphics_channel) {
        // todo: panic
        return false;
    }

    ui::backend_invalidate_objects();

    auto is_reset = tw::game::graphics::reset_device(this_);

    if(!is_reset) {
        return is_reset;
    }

    if(!plugin_update_device(this_)) { // if we still use the same device - just recreate ImGui objects
        ui::backend_create_objects();
    }

    return is_reset;
}

void __fastcall aco_set_window_mode(void* this_, bool value)
{
    if(this_ != direct_graphics_channel) {
        // todo: panic
        return;
    }

    tw::game::graphics::set_window_mode(this_, value);

    plugin_update_device(this_);
}

void __fastcall aco_set_fullscreen_device_mode(void* this_)
{
    if(this_ != direct_graphics_channel) {
        // todo: panic
        return;
    }

    tw::game::graphics::set_fullscreen_device_mode(this_);

    plugin_update_device(this_);
}

void __fastcall aco_set_view_port(void* this_, DWORD edx, D3DVIEWPORT9 viewport)
{
    (void)edx; // unused parameter, but required by the function signature

    if(this_ != direct_graphics_channel) {
        // todo: panic
        return;
    }

    tw::game::graphics::set_view_port(this_, viewport);

    plugin_update_device(this_);
}

void __fastcall aco_set_device_type(void* this_, _D3DDEVTYPE dev_type)
{
    if(this_ != direct_graphics_channel) {
        // todo: panic
        return;
    }

    tw::game::graphics::set_device_type(this_, dev_type);

    plugin_update_device(this_);
}

void __fastcall aco_invalidate_device_objects(void* this_)
{
    if(this_ != direct_graphics_channel) {
        // todo: panic
        return;
    }

    tw::game::graphics::invalidate_device_objects(this_);

    plugin_update_device(this_);
}

void __fastcall aco_set_present_window(void* this_, HWND window_handle)
{
    if(this_ != direct_graphics_channel) {
        // todo: panic
        return;
    }

    tw::game::graphics::set_present_window(this_, window_handle);

    if(plugin_update_device(this_)) {
        ui::reinit_imgui(tw::game::graphics::get_present_window(this_), tw::game::graphics::get_device(this_));
    }
}

bool __fastcall aco_create_d3d(void* this_)
{
    if(this_ != direct_graphics_channel) {
        // todo: panic;
        return false;
    }

    auto created = tw::game::graphics::create_d3d(this_);
    if(!created) {
        return created;
    }

    plugin_update_device(this_);

    return created;
}

} // namespace

namespace
{
void __fastcall aco_true_call_channel(void* this_, DWORD edx)
{
    if(initialized.load(std::memory_order_acquire)) {
        return;
    }

    // Actually we don't give a fuck what is this_ pointer because base DllInterface class placed at the top of the object anyway:
    auto engine_interface_ptr = static_cast<DllInterfaceLayout*>(this_)->engine_interface;

    aco_engine_interface = tw::game::AcoEngineInterface(engine_interface_ptr);

    auto groups_count = aco_engine_interface.get_channel_group_count();

    for(auto i { 0 }; i < groups_count; ++i) {
        auto channel_group = aco_engine_interface.get_channel_group(i);

        tw::game::AcoChannel maybe_channel { nullptr };

        // todo: find any channel of type Aco_DX8_Texture or Aco_DX8_Direct3D
        // any of it channel contains a pointer to global engine graphics channel (at the same memory location)

        if(!maybe_channel) {
            continue;
        }

        direct_graphics_channel =
            tw::game::AcoChannel(*static_cast<void**>(maybe_channel.offset_field(tw::offsets::ACO_DX8_DIRECT3D_GRAPHICS_OFFSET)));
        break;
    }

    auto device = tw::game::graphics::get_device(direct_graphics_channel);
    tw::native::change_device(device);

    std::atomic_thread_fence(std::memory_order_seq_cst);

    set_hook(tw::native::EndScene, directx_end_scene);

    // Set up a hooks to any device reconfiguration functions
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourAttach(&reinterpret_cast<PVOID&>(tw::game::graphics::reset_device), aco_reset_device);
    DetourAttach(&reinterpret_cast<PVOID&>(tw::game::graphics::set_window_mode), aco_set_window_mode);
    DetourAttach(&reinterpret_cast<PVOID&>(tw::game::graphics::set_fullscreen_device_mode), aco_set_fullscreen_device_mode);
    DetourAttach(&reinterpret_cast<PVOID&>(tw::game::graphics::set_view_port), aco_set_view_port);
    DetourAttach(&reinterpret_cast<PVOID&>(tw::game::graphics::set_device_type), aco_set_device_type);
    DetourAttach(&reinterpret_cast<PVOID&>(tw::game::graphics::invalidate_device_objects), aco_invalidate_device_objects);
    DetourAttach(&reinterpret_cast<PVOID&>(tw::game::graphics::set_present_window), aco_set_present_window);
    DetourAttach(&reinterpret_cast<PVOID&>(tw::game::graphics::create_d3d), aco_create_d3d);

    DetourTransactionCommit();

    initialized.store(true, std::memory_order_release);
}
} // namespace

// ReSharper disable once CppParameterMayBeConstPtrOrRef
unsigned __stdcall tw::plugin::load(void* thread_parameter)
{
    (void)thread_parameter;

    tw::game::graphics::initialize();
    tw::game::initialize();

    std::atomic_thread_fence(std::memory_order_seq_cst);

    auto call_channel = DetourFindFunction("HighPoly.dll", "?CallChannel@A3d_Channel@@UAEXXZ");

    tw::native::detour_attach_hook(&static_cast<PVOID&>(call_channel), aco_true_call_channel);

    return 0;
}

unsigned __stdcall tw::plugin::unload(void* thread_parameter)
{
    return 0;
}