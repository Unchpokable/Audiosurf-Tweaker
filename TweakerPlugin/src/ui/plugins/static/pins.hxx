#pragma once

#include "ui/overlay_state.hxx"

// Vertically-centered constants (active tweaks + current skin), left/right side per
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
} // namespace tw::ui::plugins::statics::pins
