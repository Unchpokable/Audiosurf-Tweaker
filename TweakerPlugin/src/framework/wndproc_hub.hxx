#pragma once

// Shared WndProc hook point. Multiple independent parts of the plugin need to see the game's
// window messages (IPC's WM_COPYDATA, D3D9's WM_ACTIVATEAPP focus tracking, ImGui's input once
// wired up) - this module owns the single SetWindowLongPtrW hook and fans messages out to whoever
// subscribed, instead of each consumer installing its own hook.
namespace tw::framework::wndproc
{
// Return true to swallow the message: dispatch stops here and `out_result` becomes the value the
// hooked WndProc returns to Windows. Return false to let dispatch continue to the next subscriber,
// then eventually to the previously-installed WndProc.
using handler_fn = bool (*)(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& out_result);

// Registers `handler` to run only when msg == this UINT (e.g. WM_COPYDATA, WM_ACTIVATEAPP).
// Subscriptions are permanent for the plugin's lifetime and independent of install()/uninstall().
void subscribe(UINT msg, handler_fn handler) noexcept;

// Registers `handler` to run for every message that reaches the hook, after all msg-specific
// subscribers for that message have had first refusal (e.g. ImGui_ImplWin32_WndProcHandler, which
// needs to see all input regardless of message type).
void subscribe_all(handler_fn handler) noexcept;

// Hooks `hwnd` via SetWindowLongPtrW(GWLP_WNDPROC). If a different window is already hooked, it is
// restored first. No-op if `hwnd` is already the currently hooked window.
void install(HWND hwnd) noexcept;

// Restores the original WndProc on the currently hooked window, if any.
void uninstall() noexcept;
} // namespace tw::framework::wndproc
