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
} // namespace tw::ui::plugins::statics::pins
