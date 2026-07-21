#pragma once

// Owns the ImGui context + DX9/Win32 backend lifecycle for the in-game overlay. Nothing in ui/
// touches ImGui_ImplDX9_*/ImGui_ImplWin32_* directly - this is the only module that does, so the
// ui/ plugins (menu, notefeed, pins, watermark) stay renderer-agnostic and reusable from the
// OpenGL smoke harness (see smoke/main.cxx, which sets up its own ImGui context the same way but
// never touches this module).
namespace tw::framework::imgui_backend
{
// Idempotent - safe to call every frame; only does real work on the first call (or after
// shutdown()). Call once a valid device + game window are known (see ui_main::draw_frame).
bool initialize(IDirect3DDevice9* device, HWND hwnd);
void shutdown();

// Mirrors ImGui_ImplDX9_Invalidate/CreateDeviceObjects - wire to
// tw::framework::d3d9::attach_device_reset_listener.
void on_lost_device();
void on_reset_device();

void new_frame();
void render();

[[nodiscard]] bool is_initialized() noexcept;

// Matches tw::framework::wndproc::handler_fn - register once via wndproc::subscribe_all(). Always
// feeds the message to ImGui (mouse/keyboard state must stay current even while the menu is
// closed), but only swallows it (returns true) when ImGui actually wants capture. Static widgets
// draw via GetBackgroundDrawList() and never set WantCaptureMouse/Keyboard, so input passes
// through to the game untouched whenever no interactive ImGui window is open. No-op (returns
// false) before initialize() has run.
bool wndproc_bridge(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& out_result);
} // namespace tw::framework::imgui_backend
