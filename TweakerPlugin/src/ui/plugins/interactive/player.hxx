#pragma once

#include "ui/qp/qp_state.hxx"

#include <imgui.h>

// The Quick Player tab of the overlay menu (Docs/Internal/overlay-quickplayer.md): playlists on the
// left, the open playlist's tracks on the right, transport and playback settings underneath.
//
// A module of its own rather than another draw_*_tab() in menu.cxx: this one owns list state,
// selection, per-row animation and (from stage 4) a set of outbound requests, and folding all of
// that into menu.cxx is what would turn it into a dumping ground.
namespace tw::ui::plugins::interactive::player
{
void initialize() noexcept;
void shutdown() noexcept;

// Draws into the current content region. Reads the snapshot only - every mutation goes out as a
// QP_NOTIFY_* request and comes back as new state (see qp_pending).
void draw(const tw::ui::qp::state::cache& snapshot);

// The content size this tab needs to be usable, in pixels. Computed from font metrics and the
// current catalog rather than measured from a previous frame, so menu.cxx can grow the window on
// the very frame the tab is opened - there is no earlier frame to measure when the tab has never
// been shown.
[[nodiscard]] ImVec2 desired_content_size(const tw::ui::qp::state::cache& snapshot);
} // namespace tw::ui::plugins::interactive::player
