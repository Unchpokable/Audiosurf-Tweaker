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

// `use_unicode_interface` picks which IDirectInputDevice8 flavor (A/W) to resolve the vtable
// against. This has nothing to do with the game *window* being ANSI or Unicode (that's a separate
// question, see wndproc_hub.cxx) - it is about which COM interface the game asks dinput8 for. The
// A default matches what Audiosurf does; the parameter exists so a game that does otherwise can be
// accommodated without touching this file.
//
// Caveat worth knowing before trusting the gate: the vtable is read off a *mouse* device, on the
// assumption that dinput8 routes every device type through the same GetDeviceState/GetDeviceData
// implementations. That holds for the dinput8.dll shipped with Windows, but it is an
// implementation detail of that DLL rather than a documented contract - if a future build splits
// them per device type, keyboard reads would stop being gated while mouse reads still are.
bool install_hooks(bool use_unicode_interface = false);
} // namespace tw::framework::dinput
