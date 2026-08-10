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
} // namespace tw::ui::plugins::statics::notefeed
