#pragma once

#include "ui/overlay_state.hxx"

// Toggleable (Insert key) custom-chrome menu window - Skins/Tweaks/Settings tabs. Fully
// self-drawn (own background/title bar/drag-move/drag-resize), no stock ImGui window chrome.
// See Docs/Internal/tweaker-plugin-widgets.md.
namespace tw::ui::plugins::interactive::menu
{
void initialize() noexcept;
void shutdown() noexcept;
void update(const tw::ui::overlay_state::cache& snapshot) noexcept;
} // namespace tw::ui::plugins::interactive::menu
