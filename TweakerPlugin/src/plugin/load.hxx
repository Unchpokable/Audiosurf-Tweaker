#pragma once

// How the plugin comes up. Docs/Internal/plugin-offline-mode.md §4.2 is the long version; the short one:
//
//   stage 0  DllMain            process check, DISABLE, mutex + Ready event, pin, mode; in the early
//                               mode also the loader notifications and the export hooks
//   stage 1  startup thread     directories, resources, every subsystem's registrations, then
//                               framework::ready; in the late mode the hooks after that
//   stage 2  first device bind  the game window's WndProc is hooked -> Ready is signalled
//   stage 3  startup thread     "startup complete", the thread ends
//
// Two modes, chosen in DllMain by whether d3d9.dll is already mapped:
//   early - loaded by the engine out of engine\channels\, before d3d9.dll. Hooks build themselves off the
//           game's own objects as it creates them.
//   late  - injected into a running game ("Load now"). Throwaway device and DirectInput objects, as before.
namespace tw::plugin
{
// Stage 0. The only thing DllMain calls. Everything it does is loader-lock safe - see the rules in §4.2.
void on_process_attach(HMODULE module) noexcept;
} // namespace tw::plugin
