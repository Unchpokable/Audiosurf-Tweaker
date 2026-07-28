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
// focus window (windowed<->exclusive-fullscreen) without recreating the device. Consumers must
// treat it as "(re)target at this device", not "first-time init".
//
// `on_unbind` fires immediately before `on_bind` whenever a *different* device replaces the current
// one, and from tw::ui::shutdown(). Consumers must release anything they hold against the outgoing
// device there and then, not defer it to a later frame.
//
// What `on_unbind` deliberately does NOT do is fire when the game simply destroys its device
// without creating a replacement - there is no Release hook to detect that (see the long note above
// resolve_d3d9_functions in the .cxx for why the one that used to exist was unusable). In practice
// that only happens at process exit, where nothing needs tearing down; the pointer held by a
// consumer goes stale but is never dereferenced again, because hk_end_scene only ever calls out for
// a device that matches the one currently bound.
void attach_device_bind_listener(device_bind_fn on_bind, device_unbind_fn on_unbind);
void detach_device_bind_listener();
} // namespace tw::framework::d3d9
