#pragma once

#include "ui/overlay_state.hxx"
#include "ui/qp/qp_state.hxx"

#include <imgui.h>

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

using extra_tab_draw_fn = void (*)();

// Appends one more tab, drawn by a module this one cannot link against, and returns its index -
// which is what extra_tab_selected() takes.
//
// The Skybox tab was the reason: it belongs to tw::skybox, which talks to a live D3D9 device, while
// this file compiles into tweaker_ui - a static library deliberately shared with smoke_test, which
// has no device and no skybox at all. A direct call would drag the whole module into a target that
// cannot build it. Registering a function pointer keeps the dependency pointing the right way, and
// leaves the tab out entirely where nobody registers one. Scripts is the second such page, hence a
// list rather than the single slot this started as.
//
// Must be called before the first update(): the tab strip is built once, on the first frame.
int add_extra_tab(std::string_view label, extra_tab_draw_fn draw) noexcept;

// Where the menu window currently is, in screen space. False when the menu is hidden, or before it
// has ever been shown and picked a position.
//
// For panels that dock to the menu rather than float on their own. Valid only after update() has run
// for the frame, which is why such a panel is drawn after it (see ui_main::draw_frame) - reading this
// earlier returns the previous frame's rectangle, and the panel lags a drag by a frame.
[[nodiscard]] bool window_rect(ImVec2& pos, ImVec2& size) noexcept;

// Whether the tab with the given handle (from add_extra_tab) is the selected one. False for -1, so
// smoke_test - where nothing registers a tab - never has to special-case it.
[[nodiscard]] bool extra_tab_selected(int handle) noexcept;
} // namespace tw::ui::plugins::interactive::menu
