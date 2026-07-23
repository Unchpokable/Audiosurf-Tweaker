#pragma once

#include "ui/overlay_state.hxx"

#include <string_view>

// Optimistic overlay -> host reverse-sync (Docs/Internal/overlay-protocol.md, "Reverse-sync:
// NOTIFY_TWEAK/NOTIFY_SKIN"). The overlay UI reflects a click instantly; this module tracks what
// was actually requested and reconciles it against the next authoritative overlay_state snapshot,
// rolling back (silently - see *_display_* below) if no matching TWEAK_SET/CURRENT_SKIN arrives
// from the host within the timeout. Renderer-agnostic (no D3D9/GL, no TweakerPlugin PCH, no direct
// tw::ipc dependency) - shared with smoke_test via a pluggable send backend, same pattern as
// ui/texture_cache.hxx's upload_fn.
namespace tw::ui::pending_actions
{
using send_fn = bool (*)(std::string_view op_line);

// Must be called once before any request_*() actually reaches the host - the DLL registers
// tw::ipc::send_overlay_command, smoke_test registers its own stub (see smoke/main.cxx). Requests
// made before a backend is registered still track pending/timeout state locally, they just never
// go out on the wire.
void set_send_backend(send_fn fn) noexcept;

// Optimistically requests a tweak flip and sends NOTIFY_TWEAK. A later call for the same id before
// confirmation lands supersedes this one (last click wins, deadline restarts) - no queue.
void request_tweak(overlay_state::tweak_id id, bool desired_enabled);

// Optimistically requests a skin change and sends NOTIFY_SKIN. Only one target skin is ever
// pending at a time - a later call supersedes the previous one.
void request_skin(std::string_view desired_name);

// Call once per frame, unconditionally - a pending action keeps ticking even while the menu that
// triggered it is closed. Reconciles every pending entry against `snapshot` (confirmed -> cleared
// silently) and expires anything past its deadline (cleared + a notefeed failure toast).
void update(const overlay_state::cache& snapshot);

// Display-layer reads: the pending optimistic value while a request is in flight, otherwise
// `snapshot`'s own value untouched. menu.cxx and pins.cxx read tweak/skin state through these
// instead of overlay_state directly, so every on-screen consumer reflects the same click.
[[nodiscard]] bool tweak_display_enabled(const overlay_state::cache& snapshot, overlay_state::tweak_id id) noexcept;
[[nodiscard]] std::string_view skin_display_name(const overlay_state::cache& snapshot) noexcept;

// Whether a skin request is currently in flight - menu.cxx uses this to disable/relabel the Skins
// tab's Apply button, since a real skin install is expensive (file unpacking on the host), unlike
// a tweak toggle: repeated clicks during the wait must not be able to queue more than one request.
[[nodiscard]] bool has_pending_skin() noexcept;

// Clears all pending state without touching overlay_state - called from tw::ipc::shutdown()
// alongside overlay_state::reset() so a stale pending request can't survive a disconnect/reconnect.
void reset() noexcept;
} // namespace tw::ui::pending_actions
