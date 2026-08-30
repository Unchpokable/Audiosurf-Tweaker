#pragma once

// Small "TweakerPlugin is loaded" badge - icon + "Audiosurf Tweaker" + FPS counter, opposite
// corner from notefeed (see overlay_config::feed_side). Always-on, non-interactive.
namespace tw::ui::plugins::statics::watermark
{
void initialize() noexcept;
void shutdown() noexcept;
void update() noexcept;

// The badge as the last update() drew it, in screen pixels. False before the first frame - the width
// depends on measured text, so there is nothing meaningful to report until it has been measured once.
[[nodiscard]] bool last_rect(float& x0, float& y0, float& x1, float& y1) noexcept;
} // namespace tw::ui::plugins::statics::watermark
