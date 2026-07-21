#pragma once

// Owns the ImGui context + DX9/Win32 backend lifecycle for the in-game overlay. Nothing in ui/
// touches ImGui_ImplDX9_*/ImGui_ImplWin32_* directly - this is the only module that does, so the
// ui/ plugins (menu, notefeed, pins, watermark) stay renderer-agnostic and reusable from the
// OpenGL smoke harness (see smoke/main.cxx, which sets up its own ImGui context the same way but
// never touches this module).
namespace tw::framework::imgui_backend
{
// Idempotent - a no-op returning true if already bound to this exact device. Safe to call again
// with a *different* device (e.g. Quest3D tore the old one down and built a replacement, or a
// Reset() changed the focus window) - internally rebinds the DX9/Win32 backends to it via unbind()
// below, without destroying the ImGui context/fonts/theme. Wire to
// tw::framework::d3d9::attach_device_bind_listener's `on_bind` so this fires exactly when the
// bound device actually changes, not once per process lifetime.
bool initialize(IDirect3DDevice9* device, HWND hwnd);

// Tears down the DX9/Win32 backends and the ImGui context itself. Only for full plugin unload -
// for a device change, use unbind() (called automatically by initialize() when needed) instead.
void shutdown();

// Tears down just the DX9/Win32 backends against the currently-bound device - keeps the ImGui
// context (fonts, theme, widget state) alive so a later initialize() is cheap. Wire to
// tw::framework::d3d9::attach_device_bind_listener's `on_unbind`: it fires from hk_release() while
// the bound device is still a valid COM object but about to hit refcount 0, so this must not defer
// any device-object release to a later frame. No-op if nothing is currently bound.
void unbind();

// Mirrors ImGui_ImplDX9_Invalidate/CreateDeviceObjects - wire to
// tw::framework::d3d9::attach_device_reset_listener.
void on_lost_device();
void on_reset_device();

void new_frame();
void render();

[[nodiscard]] bool is_initialized() noexcept;

// Matches tw::framework::wndproc::handler_fn - register once via wndproc::subscribe_all(). Always
// feeds the message to ImGui (mouse/keyboard state must stay current even while the menu is
// closed), but only swallows (returns true) actual input messages - and only when ImGui wants that
// class of input (mouse messages under WantCaptureMouse, keyboard under WantCaptureKeyboard). Every
// other message, including non-input ones like WM_NCHITTEST, is always forwarded to the game; see
// the .cxx for why swallowing WM_NCHITTEST silently kills all clicking. Static widgets draw via
// GetBackgroundDrawList() and never set WantCaptureMouse/Keyboard, so input passes through to the
// game untouched whenever no interactive ImGui window is open. No-op (returns false) before
// initialize() has run.
bool wndproc_bridge(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& out_result);
} // namespace tw::framework::imgui_backend
