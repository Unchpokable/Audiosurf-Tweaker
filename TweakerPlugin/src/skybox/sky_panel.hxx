#pragma once

#include "skybox/skybox.hxx"

// The shader parameter panel: a second overlay window, docked to the side of the menu, holding the
// knobs a sky program declares through its `@sky` annotation.
//
// It used to be a popup_menu opened from the Skybox tab. A popup was the wrong container for it in
// three separate ways: it dismisses on focus loss, while tuning a sky is a loop of "move a knob,
// look at the sky, move it again"; it shrinks to content and does not scroll, so the tab carried a
// hand-rolled BeginChild for the case where the knobs ran off the bottom of the screen; and it drew
// the `group` lines of the annotation as flat separators, which is the list-you-scroll that
// sky_params.hxx already argued against.
//
// Lives beside sky_ui.cxx and for the same reason: it is an adapter that knows about both
// tw::skybox (a live D3D9 device) and tw::ui (a static library shared with smoke_test, which has
// neither). Neither half can hold it.
namespace tw::skybox::panel
{
// The user's intent, which is not the same as whether the panel draws. Selecting a cube map hides
// the panel without clearing this, so picking a shader again brings it straight back - the whole
// point being that switching sources is not a decision to stop tuning.
void open() noexcept;
void close() noexcept;
void toggle() noexcept;

[[nodiscard]] bool open_requested() noexcept;

// Draws the window when it should be visible, and nothing otherwise. Call once per frame, after
// menu::update(): the panel docks to the menu's rectangle, which is only final once the menu has
// drawn, and it has to land above the menu when the two overlap.
void draw(const status& status) noexcept;
} // namespace tw::skybox::panel
