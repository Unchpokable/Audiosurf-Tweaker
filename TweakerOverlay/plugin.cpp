#include "pch.hpp"

#include "plugin.hpp"
#include "device.hpp"
#include "native_hooks.hpp"

#include "aco_direct_graphics.hpp"
#include "quest_offsets.hpp"
#include "quest_wrappers.hpp"

#include "ui.hpp"

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
    char* object = static_cast<char*>(this_);

    void** engine_interface = reinterpret_cast<void**>(object + engine_field_offset);

    aco_engine_interface = tw::game::AcoEngineInterface(*engine_interface);

    initialized = true;
}
}


// ReSharper disable once CppParameterMayBeConstPtrOrRef
unsigned __stdcall tw::plugin::load(void* thread_parameter)
{
    (void)thread_parameter;

    game::graphics::initialize();
    game::initialize();

    return 0;
}

unsigned __stdcall tw::plugin::unload(void* thread_parameter)
{
    return 0;
}