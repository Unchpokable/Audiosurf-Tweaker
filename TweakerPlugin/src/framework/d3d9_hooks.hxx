#pragma once

namespace tw::framework::d3d9
{
using ui_plugin_draw_fn = void (*)(IDirect3DDevice9* device);
using device_reset_listener_fn = void (*)();

bool install_d3d9_hooks();

void attach_ui_plugin(ui_plugin_draw_fn fn);
void detach_ui_plugin();

// `pre` runs just before Reset() (mirrors ImGui_ImplDX9_InvalidateDeviceObjects timing), `post`
// just after a successful Reset() (mirrors ImGui_ImplDX9_CreateDeviceObjects). Kept as raw
// function pointers, symmetric with attach_ui_plugin above, so this header stays free of any
// dependency on the ui/ module that will register them.
void attach_device_reset_listener(device_reset_listener_fn pre, device_reset_listener_fn post);
void detach_device_reset_listener();
} // namespace tw::framework::d3d9
