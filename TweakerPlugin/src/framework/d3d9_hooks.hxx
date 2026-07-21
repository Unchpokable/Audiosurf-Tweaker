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

using device_bind_fn = void (*)(IDirect3DDevice9* device, HWND hwnd);
using device_unbind_fn = void (*)();

// `on_bind` fires whenever the internally-tracked bound device/window pair actually changes: a
// brand new device from CreateDevice, the late-load path in hk_end_scene, or Reset() swapping the
// focus window (windowed<->exclusive-fullscreen) without recreating the device. Quest3D/highpoly
// can tear a device down and build a replacement without releasing the old one first, so this can
// fire again with a new device before `on_unbind` ever fires for the old one - consumers must
// treat `on_bind` as "(re)target at this device", not "first-time init".
//
// `on_unbind` fires from hk_release() for the currently-bound device, while it is still a valid
// COM object (see hk_release's AddRef/Release ordering) but about to hit refcount 0 - consumers
// must release anything they hold against it here, not defer to a later frame.
void attach_device_bind_listener(device_bind_fn on_bind, device_unbind_fn on_unbind);
void detach_device_bind_listener();
} // namespace tw::framework::d3d9
