#pragma once

// "Every subscriber is registered" - the point from which the hooks may act rather than pass through.
//
// In the early load from engine\channels\ the hooks go live before the startup thread has finished
// registering anything: Direct3DCreate9 is hooked from a loader notification ~35 ms after the plugin
// maps, while the startup thread is still loading resources and configs. Until this flag is up every hook
// only forwards to its original - no bind_device, no draw interception, no texture subscribers. The
// detours themselves are still installed as the game creates its objects; only their work waits.
//
// The subscriber vectors (d3d9 bind/reset listeners, wndproc subscribers, texture subscribers) are filled
// before publish() and never change afterwards: this flag is what publishes them to the other threads.
//
// Late mode (injected into a running game) publishes it before installing any hook, so nothing changes
// there. See Docs/Internal/plugin-offline-mode.md §4.2, "Готовность подсистем".
namespace tw::framework::ready
{
// Startup thread, once, after the last registration.
void publish() noexcept;

// Any thread. One relaxed atomic load; affordable where the hooks call it - never per draw call.
[[nodiscard]] bool published() noexcept;
} // namespace tw::framework::ready
