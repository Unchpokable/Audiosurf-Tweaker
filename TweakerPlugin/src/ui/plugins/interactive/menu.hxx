#pragma once

#include "ui/overlay_state.hxx"
#include "ui/qp/qp_state.hxx"

// Toggleable (Insert key) custom-chrome menu window - Skins/Tweaks/Settings tabs. Fully
// self-drawn (own background/title bar/drag-move/drag-resize), no stock ImGui window chrome.
// See Docs/Internal/tweaker-plugin-widgets.md.
namespace tw::ui::plugins::interactive::menu
{
void initialize() noexcept;
void shutdown() noexcept;

// Two snapshots because they are two independent models refreshed independently once per frame
// (see ui_main.cxx): the tweak/skin state and Quick Player's. Merging them into one cache would put
// a thousand playlist entries behind the same generation counter as a tweak toggle.
void update(const tw::ui::overlay_state::cache& snapshot, const tw::ui::qp::state::cache& qp_snapshot) noexcept;

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

// Opens the menu on a specific tab. Only the smoke harness uses this today, to land straight on the
// tab being worked on instead of making every run start with two clicks; the plugin itself has no
// reason to steer the user's tab choice.
void show_tab(int index) noexcept;
} // namespace tw::ui::plugins::interactive::menu
