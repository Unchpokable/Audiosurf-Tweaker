#pragma once

// What the host can learn about the plugin in a game process without talking to it - see
// Docs/Internal/plugin-offline-mode.md §5.1.
//
//   Local\AudiosurfTweaker.Overlay.<pid>        mutex, "the plugin is loaded in this process". Also what
//                                               keeps a second copy ("Load now" into a game that already
//                                               has the channels\ copy) inert.
//   Local\AudiosurfTweaker.Overlay.<pid>.Ready  manual-reset event, signalled once the plugin accepts
//                                               TW_OVL: the game window's WndProc is hooked.
//
// The names carry no version and no path on purpose: any host must find any plugin. Both handles live
// until the process exits, which is exactly the lifetime they describe.
namespace tw::plugin::presence
{
enum class claim_result : std::uint8_t {
    // This copy owns the process. The Ready event exists, not signalled.
    claimed,
    // Another copy is already loaded. This one must stay inert.
    already_loaded,
    // The mutex could not be created. Presence cannot be proven either way; carrying on is the lesser evil.
    unavailable,
};

// From DllMain. Loader-lock safe: CreateMutexW and CreateEventW only.
claim_result claim() noexcept;

// Signals Ready. Idempotent; from the first device bind (initialisation, where a syscall is allowed).
void signal_ready() noexcept;
} // namespace tw::plugin::presence
