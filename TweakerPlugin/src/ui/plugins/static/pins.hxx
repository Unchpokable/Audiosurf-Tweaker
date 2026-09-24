#pragma once

#include "ui/overlay_state.hxx"

// Vertically-centered constants (active tweaks + current skin, or an "Offline" row while Audiosurf
// Tweaker is not connected and overlay_config::offline_pin() is on), left/right side per
// overlay_config::pins_side(). Always-on, non-interactive - no push API, derives its content
// straight from the latest overlay_state snapshot every frame.
namespace tw::ui::plugins::statics::pins
{
void initialize() noexcept;
void shutdown() noexcept;
void update(const tw::ui::overlay_state::cache& snapshot) noexcept;

// The block of pins drawn by the last update(), in screen pixels. False when nothing was drawn -
// which is the normal state, since pins only exist while a tweak is on or a skin is applied, and
// the block's width follows the longest label.
//
// Unlike notefeed's column, this one is reported as it actually is rather than as a reserved strip:
// a consumer laying out around it has to react to it appearing and disappearing anyway, so a
// truthful rectangle is more useful than a conservative one.
[[nodiscard]] bool last_rect(float& x0, float& y0, float& x1, float& y1) noexcept;

// One extra muted row, above the rest, or none when empty. The single exception to "no push API",
// and it is narrow on purpose: it carries a status the overlay state cannot know because it is not
// about the host at all - the scripting layer waiting for the game to finish loading.
//
// It lives here rather than being derived like everything else because this layer is shared with
// smoke_test, which has no engine, no channel graph and no scripts. Deriving it would mean ui/
// including engine/, and that dependency is exactly what the shared build cannot have.
//
// The string is copied. Setting the same text twice is free.
void set_status(std::string_view text) noexcept;
} // namespace tw::ui::plugins::statics::pins
