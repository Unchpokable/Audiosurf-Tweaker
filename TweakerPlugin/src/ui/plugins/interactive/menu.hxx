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

// Used by ui_main.cxx to gate BOTH input paths (see framework/dinput8_hooks.hxx and
// framework/imgui_backend.hxx): while the menu is open, the game's raw device reads come back
// zeroed and its window messages are swallowed, so ImGui owns keyboard/mouse input exclusively
// instead of the game also reacting to menu clicks/typing as gameplay input. While it is closed,
// ImGui is fed nothing at all and the game's input is untouched.
//
// Called from whichever thread delivers window messages as well as from the render thread, hence
// the atomic behind it.
bool is_visible() noexcept;

// Toggles/sets visibility. The hotkey that drives this lives with the message pump, not here:
// the DLL subscribes VK_INSERT on the wndproc hub (ui_main.cxx), the smoke harness does the same
// in its own WndProc (smoke/main.cxx). This module deliberately does not poll the keyboard - a
// GetAsyncKeyState poll only runs on frames the overlay actually draws, sees keys the window never
// received, and is at the mercy of whatever low-level input hooks DirectInput has installed.
void toggle_visible() noexcept;
void set_visible(bool visible) noexcept;
} // namespace tw::ui::plugins::interactive::menu
