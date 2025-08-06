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
struct AcoTextureLayout
{
    // 88 is magic constant extracted from IDA decompiler from Aco_DX8_Texture::GetTexture. It only does `mov eax, [ecx+88h]`
    // if ecx contains `this` pointer, then 88h is an offset to field IDirect3DTexture9* texture according to its header:
    // private:
    //      int referenceNr_; // scrap
    //      IDirect3DTexture9 texture_; // should be 88 offset from `this`
    //      IDirect3DTexture9 bumpMapTexture_; // who's tf you are lol
    //      Aco_DX8_DirectGraphicsChannel* uniqueGraphics_; // here you are
    // SOOOOOOOOOO
    char pad1[88];                      // this is an offset to texture
    void* pad2;                         // this is a strange fucker
    void* aco_unique_directx_channel;   // this is - our target
};
}

namespace
{
bool initialized { false };
}

namespace
{
tw::game::AcoEngineInterface aco_engine_interface { nullptr };
tw::game::AcoChannel unique_graphics_channel { nullptr };
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
void __fastcall aco_true_call_channel(void* this_, DWORD edx)
{
    if(initialized) {
        return;
    }

    // lets assume that this_ is a pointer to A3d_Channel, then:
    auto engine_interface = reinterpret_cast<DllInterfaceLayout*>(this_)->engine_interface;

    aco_engine_interface = tw::game::AcoEngineInterface(engine_interface);

    auto groups_count = aco_engine_interface.get_channel_group_count();

    for(auto i { 0 }; i < groups_count; ++i) {
        auto channel_group = aco_engine_interface.get_channel_group(i);

        auto channel_count = channel_group.get_unique_channel_count();

        for(auto j { 0 }; j < channel_count; ++j) {
            auto channel = channel_group.get_unique_channel(j);
        }
    }

    initialized = true;
}
}


// ReSharper disable once CppParameterMayBeConstPtrOrRef
unsigned __stdcall tw::plugin::load(void* thread_parameter)
{
    (void)thread_parameter;

    game::graphics::initialize();
    game::initialize();

    auto call_channel = DetourFindFunction("HighPoly.dll", "?CallChannel@A3d_Channel@@UAEXXZ");

    native::detour_attach_hook(&static_cast<PVOID&>(call_channel), aco_true_call_channel);

    return 0;
}

unsigned __stdcall tw::plugin::unload(void* thread_parameter)
{
    return 0;
}