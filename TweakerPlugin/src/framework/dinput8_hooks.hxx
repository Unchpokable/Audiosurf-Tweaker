#pragma once

// Detours-based hooks on IDirectInputDevice8::GetDeviceState/GetDeviceData, patched process-wide -
// DirectInput8 dispatches every device object (mouse/keyboard/joystick) the game creates through the
// same vtable functions, so one attach covers all of them.
//
// Where the vtable comes from depends on how the plugin got into the process:
//   - early (engine\channels\): off the game's OWN objects, as it creates them - DirectInput8Create is
//     hooked from DllMain, which hooks IDirectInput8::CreateDevice on the first object of each kind (A/W),
//     which hooks GetDeviceState/GetDeviceData on the first device. No throwaway objects at all.
//   - late (injected): off a throwaway device, install_hooks() below.
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

// Early load only, from DllMain: detours the DirectInput8Create export without suspending threads. Safe
// there because the plugin imports dinput8.dll statically (so it is mapped) and the game first calls the
// export ~0.6 s later (plugin-offline-mode.md, Р-24). The rest of the chain installs itself from the game's
// own calls, on the game's thread, outside the loader lock.
bool install_create_hook_from_loader() noexcept;
} // namespace tw::framework::dinput
