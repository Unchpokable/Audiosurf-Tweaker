#pragma once

namespace tw::game::graphics
{
using get_direct3d_func = IDirect3D9*(__thiscall*)(void* this_);
extern get_direct3d_func get_direct3d;

using get_device_func = IDirect3DDevice9*(__thiscall*)(void* this_);
extern get_device_func get_device;

using reset_device_func = bool(__thiscall*)(void* this_);
extern reset_device_func reset_device;

using set_render_state_func = void(__thiscall*)(void* this_, std::uint32_t state, std::uint32_t value);
extern set_render_state_func set_render_state;

using set_texture_stage_state_func = void(__thiscall*)(void* this_, std::uint32_t state, D3DTEXTURESTAGESTATETYPE type, std::uint32_t value);
extern set_texture_stage_state_func set_texture_state_stage;

using set_sampler_state_func = void(__thiscall*)(void* this_, std::uint32_t state, D3DSAMPLERSTATETYPE type, std::uint32_t value);
extern set_sampler_state_func set_sampler_state;

using set_device_states_func = bool(__thiscall*)(void* this_);
extern set_device_states_func set_device_states;

using check_device_restore_func = void(__thiscall*)(void* this_);
extern check_device_restore_func check_device_restore;

using set_window_mode_func = void(__thiscall*)(void* this_, bool value);
extern set_window_mode_func set_window_mode;

using set_fullscreen_device_mode_func = void(__thiscall*)(void* this_);
extern set_fullscreen_device_mode_func set_fullscreen_device_mode;

using set_view_port_func = void(__thiscall*)(void* this_, D3DVIEWPORT9 viewport);
extern set_view_port_func set_view_port;

using invalidate_device_objects_func = void(__thiscall*)(void* this_);
extern invalidate_device_objects_func invalidate_device_objects;

using set_device_type_func = void(__thiscall*)(void* this_, _D3DDEVTYPE dev_type);
extern set_device_type_func set_device_type;

using set_present_window_func = void(__thiscall*)(void* this_, HWND window_handle);
extern set_present_window_func set_present_window;

using create_d3d_func = bool(__thiscall*)(void* this_);
extern create_d3d_func create_d3d;

using get_present_window_func = HWND(__thiscall*)(void* this_);
extern get_present_window_func get_present_window;
} // namespace

namespace tw::game::graphics
{
void initialize();
} // namespace tw::game

namespace tw::game::graphics
{
bool is_fine();
}
