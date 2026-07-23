#pragma once

// Detours-based hooks on IDirectInputDevice8::GetDeviceState/GetDeviceData. Both are resolved once
// off a throwaway device instance and patched process-wide - DirectInput8 dispatches every device
// object (mouse/keyboard/joystick) the game creates through the same vtable functions, so one
// attach covers all of them.
//
// The gate lets a consumer (currently the ImGui menu, see menu.cxx) swallow real input while it's
// open: the original is always still called so DirectInput's internal state/buffer keeps draining,
// but the result handed back to the game is zeroed out whenever the gate reports true. No
// Windows/DirectInput types appear in this header on purpose, so it stays includable from
// PCH-less translation units (menu.cxx).
namespace tw::framework::dinput
{
using input_gate_fn = bool (*)();

void attach_input_gate(input_gate_fn fn) noexcept;
void detach_input_gate() noexcept;

// `is_window_unicode` picks which IDirectInputDevice8 flavor (A/W) to resolve against - must match
// what the game itself creates. Quest3D registers its window ANSI (see wndproc_hub.cxx), hence the
// default.
bool install_hooks(bool is_window_unicode = false);
} // namespace tw::framework::dinput
