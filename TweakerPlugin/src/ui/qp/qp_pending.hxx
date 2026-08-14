#pragma once

#include "ui/overlay_state.hxx"
#include "ui/qp/qp_catalog.hxx"
#include "ui/qp/qp_state.hxx"

#include <string_view>

// Optimistic overlay -> host reverse-sync for Quick Player, the same three rules ui/pending_actions
// applies to tweaks and skins (Docs/Internal/overlay-protocol.md, "Reverse-sync"):
//
//   1. a click shows its result immediately and sends a QP_NOTIFY_* request;
//   2. every frame, a pending request is reconciled against the newest authoritative snapshot -
//      matching means confirmed, and it is dropped silently because the UI already showed it;
//   3. past the deadline it is dropped too, and the display falls straight back to whatever the
//      snapshot actually holds, which is the rollback - there is no separate undo step.
//
// A module of its own rather than an extension of pending_actions: that one is built around a fixed
// array of seven tweaks plus one skin, addressed by a compile-time index, and none of Quick
// Player's requests fit that shape.
namespace tw::ui::qp::pending
{
// Requests. A second request of the same kind before the first is confirmed replaces it - last
// click wins, deadline restarts, no queue.
void request_mode(std::string_view playlist_id, playback_mode mode);
void request_advance(std::string_view playlist_id, advance_trigger trigger);
void request_default_character(character_id id);
void request_play(std::string_view playlist_id, std::string_view entry_id);

// "stop", "next" or "prev". `playing_entry_id` is what was playing when the button was pressed:
// for next/prev the overlay cannot know which entry the host will land on (it holds the upcoming
// queue, but a skip past a missing file can pass over it), so the request is confirmed by the
// playing entry *changing* rather than by it reaching a predicted value.
void request_transport(std::string_view action, std::string_view playing_entry_id);

// Per-track edits, keyed by entry. Several can be in flight at once - unlike the single-slot
// requests above, they address different things and must not evict each other.
void request_tag(std::string_view entry_id, tag_id id, bool enabled, int parameter, bool has_parameter);
void request_mod(std::string_view entry_id, overlay_state::tweak_id id, bool enabled);
void request_character(std::string_view entry_id, character_id id);

// Call once per frame, unconditionally - a request keeps ticking while the menu that started it is
// closed, exactly as pending_actions does.
void update(const state::cache& snapshot);

// Display-layer reads: the pending value while a request is in flight, otherwise the snapshot's own
// value untouched. Everything on screen goes through these, so a click is reflected everywhere at
// once and rolls back everywhere at once.
[[nodiscard]] playback_mode display_mode(const state::cache& snapshot) noexcept;
[[nodiscard]] advance_trigger display_advance(const state::cache& snapshot) noexcept;
[[nodiscard]] character_id display_default_character(const state::cache& snapshot) noexcept;
[[nodiscard]] std::string_view display_playing_entry(const state::cache& snapshot) noexcept;

// Per-track display. `entry` is the authoritative row; the return value is what to draw for it.
[[nodiscard]] bool display_tag_enabled(const state::track& entry, tag_id id) noexcept;
[[nodiscard]] int display_tag_parameter(const state::track& entry, tag_id id) noexcept;
[[nodiscard]] bool display_mod_enabled(const state::track& entry, overlay_state::tweak_id id) noexcept;
[[nodiscard]] character_id display_character(const state::track& entry) noexcept;

// Whether anything at all is in flight, so the transport can show that it is waiting rather than
// looking like it ignored the click.
[[nodiscard]] bool has_pending_transport() noexcept;

void reset() noexcept;
} // namespace tw::ui::qp::pending
