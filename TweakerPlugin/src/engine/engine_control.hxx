#pragma once

#include "framework/detour_transaction.hxx"

// The spine: one detour on `EngineControl::EngineLoop`, and everything that follows from having it.
//
// `EngineLoop` is what QuestViewer.exe's message pump calls once per frame, and the only place the
// channel graph is entered from - inside it the engine bumps its tree count exactly once and calls
// `CallStartChannel` on exactly one group, the start group; everything else in the graph is reached
// by following links from there (Docs/Internal/reversing-journal-boot.md §1.2).
//
// Three things come out of that, and the plugin wanted all three:
//
//  - **a real per-frame point on the engine thread**, before and after the whole graph, that does
//    not depend on there being a device or on the window being visible. The overlay's Present is
//    neither of those things, and until now it was the only tick the scripting layer had;
//  - **`EngineInterface*` on the first frame**, out of `this`, with no waiting. The old way of
//    getting it - a detour on the empty `A3d_Channel::CallChannel` - fired only when the game
//    happened to evaluate a channel that does not override it, which in practice could mean "after
//    the player clicks something in the menu" (engine journal §7, fact 4);
//  - **the removal of that detour's cost**, which was a trampoline plus a null check on every call
//    of every one of 22 655 channels, for the whole session, after the one moment it was useful.
//
// **The identity of the object is checked, not assumed.** `??_7EngineControl@@6B@` is exported, so
// on the first call the vptr of `this` is compared against it. Everything in this module was read
// out of a disassembler and never run against the game before Ф1; if the class turns out to be a
// different one, that check is what turns a wrong guess into a log line instead of a crash.
namespace tw::engine::control
{
// Runs on the engine thread, inside the game's own call stack, once per engine frame. Hot path: no
// allocation, no logging, no locks.
using frame_fn = void (*)() noexcept;

// Installs the detour. Cold, once. False means the symbol is missing or Detours refused, and the
// caller is expected to fall back (see plugin/load.cxx).
//
// **The `threads` argument is not a preference, it is the difference between two loading modes.**
//
//  - Early load: called from DllMain with `suspend::none`, next to the other stage 0 hooks. Nobody
//    can be inside EngineLoop yet - the engine is still enumerating engine\channels\, which is what
//    loaded us, and its message pump has not started. Suspending a thread from there is the exact
//    deadlock plugin-offline-mode.md §4.2 forbids: the game's thread is inside the loader, holding
//    locks that Detours' own allocation would then wait on.
//  - Late load (injected into a running game): called from the startup thread with the default
//    `suspend::others`, because there the game's main loop is calling EngineLoop right now and
//    patching its first bytes underneath it is the thing that must not happen.
//
// Subscribers may be registered after install(): the hook does not dispatch to them until
// framework::ready is published, which is what makes the lists safe to fill from another thread.
bool install(framework::detour::suspend threads = framework::detour::suspend::others) noexcept;

[[nodiscard]] bool installed() noexcept;

// Whether an EngineLoop has actually arrived. The difference between this and installed() is the
// whole question Ф1 exists to answer, so load.cxx reports both.
[[nodiscard]] bool ticked() noexcept;

// Whether the first frame's identity check passed. False before the first frame, and false forever
// after a failed check - in which case the detour stays in but does nothing except forward.
[[nodiscard]] bool healthy() noexcept;

// Before the graph and after it. Registered from the startup thread, before install(); the lists are
// read from the engine thread afterwards and never change again, which is what makes them free of
// synchronisation. Silently ignored past the (small, fixed) capacity - subscribers are plugin
// modules, not scripts, and there is a compile-time handful of them.
void subscribe_pre(frame_fn fn) noexcept;
void subscribe_post(frame_fn fn) noexcept;
} // namespace tw::engine::control
