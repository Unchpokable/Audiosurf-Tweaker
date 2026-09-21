#pragma once

#include "ui/overlay_state.hxx"

// The render thread's side of "Audiosurf Tweaker connected / left" (Docs/Internal/plugin-offline-mode.md
// §4.5). Whoever notices the host - tw::ipc on handshake, HOST_DISCONNECT, the watchdog or a failed send -
// flips overlay_state::set_host_connected(); the state that belongs to the render thread is reset here,
// when the new value reaches its snapshot.
//
// Renderer-agnostic and PCH-free like the rest of tweaker_ui: shared with smoke_test, which drives the same
// edges from a checkbox.
namespace tw::ui::host_link
{
enum class edge : std::uint8_t {
    none,
    connected,
    disconnected,
};

// Once per frame, right after overlay_state::refresh() and before pending_actions::update(). Compares the
// snapshot with last frame's: on either edge drops every in-flight request (pending_actions,
// qp::pending) - a request nobody will ever answer must not end in a "failed" toast five seconds later -
// and raises the connect/disconnect toast. The first call only seeds: starting offline is not a
// transition. A bool compare per frame and nothing else.
edge update(const overlay_state::cache& snapshot) noexcept;
} // namespace tw::ui::host_link
