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

// Used by ui_main.cxx to gate the dinput8 hooks (see framework/dinput8_hooks.hxx) - while the menu
// is open, the game's raw device reads come back zeroed so ImGui owns keyboard/mouse input
// exclusively instead of the game also reacting to menu clicks/typing as gameplay input. ImGui
// itself never goes through dinput - it gets input from the wndproc hub - so gating purely on
// visibility is safe.
bool is_visible() noexcept;
} // namespace tw::ui::plugins::interactive::menu
