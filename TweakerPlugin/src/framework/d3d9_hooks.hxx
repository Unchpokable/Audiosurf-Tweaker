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
//
// Additive: every subscriber is kept and run in registration order. The overlay is no longer the
// only consumer (tw::skybox holds device resources of its own), and a second attach silently
// replacing the first would have been a device-resource leak that only shows up on an alt-tab.
void attach_device_reset_listener(device_reset_listener_fn pre, device_reset_listener_fn post);
void detach_device_reset_listener(device_reset_listener_fn pre, device_reset_listener_fn post);

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
//
// Additive, like the reset listener above. Unbind subscribers run in reverse registration order, so
// a consumer registered later (and therefore possibly built on top of an earlier one) tears down
// first.
void attach_device_bind_listener(device_bind_fn on_bind, device_unbind_fn on_unbind);
void detach_device_bind_listener(device_bind_fn on_bind, device_unbind_fn on_unbind);

// Called from hk_draw_primitive / hk_draw_indexed_primitive, before the game's own draw runs, with
// whatever texture is currently bound to stage 0 (tracked by hk_set_texture, so this costs no COM
// call on the hot path). Return true to say "I drew something else instead" - the game's draw is
// then skipped entirely and the hook reports D3D_OK.
//
// This runs on EVERY draw call the game makes, so the interceptor's own reject path has to be a
// couple of comparisons and nothing more.
//
// Re-entrancy is handled here, not by the interceptor: a draw issued from inside the interceptor
// lands right back in these hooks, and is passed straight through to the original.
//
// The interceptor must leave the device exactly as it found it - a D3DSBT_ALL state block is the
// intended way. Stage-0 tracking is suspended for the duration of the call precisely because a
// state block restores bindings behind D3D's back, without passing through hk_set_texture.
using draw_intercept_fn = bool (*)(IDirect3DDevice9* device, IDirect3DBaseTexture9* stage0_texture);

void attach_draw_interceptor(draw_intercept_fn fn);
void detach_draw_interceptor();
} // namespace tw::framework::d3d9
