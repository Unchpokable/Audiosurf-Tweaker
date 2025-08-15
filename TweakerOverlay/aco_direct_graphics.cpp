#include "pch.hpp"

#include "aco_direct_graphics.hpp"

namespace tw::game::graphics
{
get_direct3d_func get_direct3d {};
get_device_func get_device {};
reset_device_func reset_device {};
set_render_state_func set_render_state {};
set_texture_stage_state_func set_texture_state_stage {};
set_sampler_state_func set_sampler_state {};
set_device_states_func set_device_states {};
check_device_restore_func check_device_restore {};
set_window_mode_func set_window_mode {};
set_fullscreen_device_mode_func set_fullscreen_device_mode {};
set_view_port_func set_view_port {};
invalidate_device_objects_func invalidate_device_objects {};
set_device_type_func set_device_type {};
set_present_window_func set_present_window {};
create_d3d_func create_d3d {};
get_present_window_func get_present_window {};
} // namespace tw::game::graphics

void tw::game::graphics::initialize()
{
    static constexpr char module[] = "E2D1C95B-1B84-4D94-A373-BEBABADF7AEE.dll";
    get_direct3d = reinterpret_cast<get_direct3d_func>(
        DetourFindFunction(module, "?GetDirect3d@Aco_DX8_DirectGraphicsChannel@@UAEPAUIDirect3D9@@XZ"));

    get_device = reinterpret_cast<get_device_func>(
        DetourFindFunction(module, "?GetDirect3dDevice@Aco_DX8_DirectGraphicsChannel@@UAEPAUIDirect3DDevice9@@XZ"));

    reset_device = reinterpret_cast<reset_device_func>(DetourFindFunction(module, "?ResetDevice@Aco_DX8_DirectGraphicsChannel@@UAE_NXZ"));

    set_render_state =
        reinterpret_cast<set_render_state_func>(DetourFindFunction(module, "?SetRenderState@Aco_DX8_DirectGraphicsChannel@@UAEXKK@Z"));

    set_texture_state_stage = reinterpret_cast<set_texture_stage_state_func>(
        DetourFindFunction(module, "?SetTextureStageState@Aco_DX8_DirectGraphicsChannel@@UAEXKW4_D3DTEXTURESTAGESTATETYPE@@K@Z"));

    set_sampler_state = reinterpret_cast<set_sampler_state_func>(
        DetourFindFunction(module, "?SetSamplerState@Aco_DX8_DirectGraphicsChannel@@UAEXKW4_D3DSAMPLERSTATETYPE@@K@Z"));

    set_device_states =
        reinterpret_cast<set_device_states_func>(DetourFindFunction(module, "?SetDeviceStates@Aco_DX8_DirectGraphicsChannel@@UAE_NXZ"));

    check_device_restore = reinterpret_cast<check_device_restore_func>(
        DetourFindFunction(module, "?CheckDeviceRestore@Aco_DX8_DirectGraphicsChannel@@UAEXXZ"));

    set_window_mode = reinterpret_cast<set_window_mode_func>(
        DetourFindFunction(module, "?SetWindowMode@Aco_DX8_DirectGraphicsChannel@@UAEX_N@Z"));

    set_view_port = reinterpret_cast<set_view_port_func>(
        DetourFindFunction(module, "?SetViewPort@Aco_DX8_DirectGraphicsChannel@@UAEXU_D3DVIEWPORT9@@@Z"));

    set_fullscreen_device_mode = reinterpret_cast<set_fullscreen_device_mode_func>(
        DetourFindFunction(module, "?SetFullScreenDeviceMode@Aco_DX8_DirectGraphicsChannel@@UAEXU_D3DDISPLAYMODE@@H_N1@Z"));

    invalidate_device_objects = reinterpret_cast<invalidate_device_objects_func>(
        DetourFindFunction(module, "?InvalidateDeviceObjects@Aco_DX8_DirectGraphicsChannel@@UAEXXZ"));

    set_device_type = reinterpret_cast<set_device_type_func>(
        DetourFindFunction(module, "?SetDeviceType@Aco_DX8_DirectGraphicsChannel@@UAEXW4_D3DDEVTYPE@@@Z"));

    set_present_window = reinterpret_cast<set_present_window_func>(
        DetourFindFunction(module, "?SetPresentWindow@Aco_DX8_DirectGraphicsChannel@@UAEXPAUHWND__@@@Z"));

    create_d3d = reinterpret_cast<create_d3d_func>(DetourFindFunction(module, "?CreateD3D@Aco_DX8_DirectGraphicsChannel@@UAE_NXZ"));

    get_present_window = reinterpret_cast<get_present_window_func>(
        DetourFindFunction(module, "?GetPresentWindow@Aco_DX8_DirectGraphicsChannel@@UAEPAUHWND__@@XZ"));

    assert(get_direct3d);
    assert(get_device);
    assert(reset_device);
    assert(set_render_state);
    assert(set_texture_state_stage);
    assert(set_sampler_state);
    assert(set_device_states);
    assert(check_device_restore);
    assert(set_window_mode);
    assert(set_view_port);
    assert(set_fullscreen_device_mode);
    assert(invalidate_device_objects);
    assert(set_device_type);
    assert(set_present_window);
    assert(create_d3d);
    assert(get_present_window);
}

bool tw::game::graphics::is_fine()
{
    return get_direct3d
        && get_device
        && reset_device
        && set_render_state
        && set_texture_state_stage
        && set_sampler_state
        && set_device_states
        && check_device_restore
        && set_window_mode
        && set_view_port
        && set_fullscreen_device_mode
        && invalidate_device_objects
        && set_device_type
        && set_present_window
        && create_d3d;
}
