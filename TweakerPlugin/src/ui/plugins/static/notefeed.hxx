#pragma once

#include <string_view>

// Toast notifications, top-left/top-right corner (see overlay_config::feed_side). Always-on,
// non-interactive - drawn via ImGui::GetBackgroundDrawList(), never an ImGui window.
namespace tw::ui::plugins::statics::notefeed
{
void initialize() noexcept;
void shutdown() noexcept;
void update() noexcept;

// "Fire and forget" - any part of the plugin can call this and never think about the
// notification again; notefeed owns its own lifetime/animation from here (see
// Docs/Internal/tweaker-plugin-widgets.md). `icon_resource_key` is an optional ui/image/svg key
// (e.g. "icons/skin.svg", or overlay_state::tweak_icon_key(id)) - empty means text-only.
void push(std::string_view text, std::string_view icon_resource_key = {});

// The screen rectangle toasts may occupy, in pixels: the feed's column, three rows deep. Reported
// whether or not anything is currently showing, and deliberately so - a consumer laying itself out
// around the feed must not shift every time a toast appears and expires.
//
// Three rows rather than the full column height: reserving the whole side of the screen made every
// consumer that respects this shove itself sideways for space that is almost never used.
//
// Exists for the scripting layer (tw.hud.safe in src/lua), which draws into the same background
// list but earlier in the frame, and therefore ends up underneath. Rather than let a script guess,
// or reorder the overlay's own chrome behind user content, it gets told where not to draw.
void reserved_rect(float& x0, float& y0, float& x1, float& y1) noexcept;
} // namespace tw::ui::plugins::statics::notefeed
