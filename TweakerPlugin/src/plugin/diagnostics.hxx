#pragma once

// Diagnostic logging for the injected DLL.
//
// The interesting failures in this plugin are ordering failures: which hook fired first, whether
// bind_device ran before the first EndScene, whether unbind_device ever runs at all, whether the
// input gate opened when the menu did. None of that is reconstructible from a crash dump, so the
// plugin logs its state transitions - and, in debug builds, spawns a console on the game process
// to read them live.
//
// Release builds compile ALL of it away. TW_LOG_* below expand to `((void)0)` the moment any
// release macro is defined, so the format string, the argument evaluation, the std::format call
// (which allocates, takes a mutex and can throw) and the sink dispatch all disappear - nothing is
// left but the code that was being logged about. That is what makes these macros safe to use in
// hot-path code (EndScene, CallChannel, the dinput hooks, the WndProc hub): in the build that
// ships, they are not there at all.
//
// Levels are ordinary uulog levels, forwarded verbatim in debug builds:
//   TW_LOG_DEBUG    - per-frame / per-message chatter, hot-path detail
//   TW_LOG_INFO     - lifecycle transitions worth seeing on every run
//   TW_LOG_WARNING  - something unexpected the plugin recovered from
//   TW_LOG_ERROR    - a subsystem failed to come up
//   TW_LOG_CRITICAL - about to crash / invariant broken

#if defined(NDEBUG) || defined(_NDEBUG) || defined(RELEASE)
#define TW_DIAGNOSTICS_ENABLED 0
#else
#define TW_DIAGNOSTICS_ENABLED 1
#endif

#if TW_DIAGNOSTICS_ENABLED
// Calls uulog directly instead of forwarding to its own LOG_* macros. Forwarding would mean
// `#define TW_LOG_INFO(...) LOG_INFO(__VA_ARGS__)`, and MSVC's traditional preprocessor (which
// this project builds with) hands a forwarded __VA_ARGS__ to the next macro as a *single*
// argument - so `TW_LOG_INFO("x={}", x)` arrives at LOG_INFO with fmt = `"x={}", x`, a comma
// expression that evaluates to x. The format string then isn't a constant expression and the
// arguments are gone. Expanding straight into the function call sidesteps the whole problem: the
// variadic text is pasted into an ordinary call, where commas mean what they say.
#define TW_LOG_DEBUG(...)    uulog::debug(__FILE__, __LINE__, __VA_ARGS__)
#define TW_LOG_INFO(...)     uulog::info(__FILE__, __LINE__, __VA_ARGS__)
#define TW_LOG_WARNING(...)  uulog::warning(__FILE__, __LINE__, __VA_ARGS__)
#define TW_LOG_ERROR(...)    uulog::error(__FILE__, __LINE__, __VA_ARGS__)
#define TW_LOG_CRITICAL(...) uulog::critical(__FILE__, __LINE__, __VA_ARGS__)
#else
#define TW_LOG_DEBUG(...)    ((void)0)
#define TW_LOG_INFO(...)     ((void)0)
#define TW_LOG_WARNING(...)  ((void)0)
#define TW_LOG_ERROR(...)    ((void)0)
#define TW_LOG_CRITICAL(...) ((void)0)
#endif

namespace tw::plugin::diagnostics
{
// Debug: allocates a console on the game process (unless it already has one), points stdout at it,
// enables ANSI colors, and registers the colored stdout sink plus the OutputDebugString sink.
// Release: does nothing at all - no console, no sinks, no allocation.
void initialize() noexcept;

// Detaches the sinks and frees a console we allocated ourselves. Never called today (the plugin
// has no unload path, see CLAUDE.md) but kept symmetric so it exists when one appears.
void shutdown() noexcept;
} // namespace tw::plugin::diagnostics
