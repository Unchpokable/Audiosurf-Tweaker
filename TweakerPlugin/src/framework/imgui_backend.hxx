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

// Decides whether ImGui is allowed to see input at all - wire to menu::is_visible via
// attach_input_gate() (see ui_main.cxx). Mirrors tw::framework::dinput::input_gate_fn so both
// input paths are gated on the same predicate and cannot drift apart.
using input_gate_fn = bool (*)();

void attach_input_gate(input_gate_fn fn) noexcept;
void detach_input_gate() noexcept;

// Matches tw::framework::wndproc::handler_fn - register once via wndproc::subscribe_all().
//
// While the gate is closed (menu hidden) ImGui is fed *nothing* except the few messages that carry
// no Win32 side effects (focus, IME language, window destruction), and every message is forwarded
// to the game untouched. That is deliberate and not merely an optimization:
// ImGui_ImplWin32_WndProcHandler is not a passive observer - it calls ::SetCapture on any mouse
// button down, ::ReleaseCapture on button up, and ::TrackMouseEvent on mouse move. Handing it the
// game's messages while the overlay is idle means ImGui releases mouse capture the game itself had
// taken (mouse-look, drag gestures), and registers WM_MOUSELEAVE tracking the game never asked
// for. Mouse position does not suffer from being cut off: ImGui_ImplWin32_NewFrame polls
// ::GetCursorPos every frame whenever no WM_MOUSEMOVE tracking is active, so the cursor is already
// correct on the first frame after the menu opens.
//
// While the gate is open, ImGui sees everything and every input-class message is swallowed - same
// contract as the dinput8 gate, which zeroes the game's raw device reads over exactly the same
// interval. Non-input messages (WM_NCHITTEST, WM_SETCURSOR, WM_MOUSEACTIVATE, ...) are still always
// forwarded; see the .cxx for why swallowing WM_NCHITTEST silently kills all clicking.
//
// No-op (returns false) before initialize() has run.
bool wndproc_bridge(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& out_result);
} // namespace tw::framework::imgui_backend
