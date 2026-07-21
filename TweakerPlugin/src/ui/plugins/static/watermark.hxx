#pragma once

// Small "TweakerPlugin is loaded" badge - icon + "Audiosurf Tweaker" + FPS counter, opposite
// corner from notefeed (see overlay_config::feed_side). Always-on, non-interactive.
namespace tw::ui::plugins::statics::watermark
{
void initialize() noexcept;
void shutdown() noexcept;
void update() noexcept;
} // namespace tw::ui::plugins::statics::watermark
