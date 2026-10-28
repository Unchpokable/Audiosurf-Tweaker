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

// The box around the toasts drawn by the last update(), in screen pixels - fading ones included, and
// following the stack as it moves. False when there are none, which is most of the time.
//
// What the scripting layer reports as the feed (tw.hud.widget, occupied, place), the way pins and the
// watermark report themselves as they are. It used to report reserved_rect() instead, so that a script
// laid out around the feed would not move when a toast came and went - and the price was every script
// keeping clear of a corner that is empty almost always. Scripts draw after the overlay's own widgets
// (ui_main.cxx), so this is final for the frame by the time a script asks.
[[nodiscard]] bool last_rect(float& x0, float& y0, float& x1, float& y1) noexcept;

// Where toasts may go, whether or not any are showing: the feed's column, a fixed number of single-line
// toasts deep at the current font size. Only for tw.hud.safe(), the coarse "somewhere uncluttered"
// answer, which is a stable band by definition.
void reserved_rect(float& x0, float& y0, float& x1, float& y1) noexcept;
} // namespace tw::ui::plugins::statics::notefeed
