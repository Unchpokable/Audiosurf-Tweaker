#pragma once

// The overlay's Skybox tab, and the notifications that go with it.
//
// Its own translation unit, and on the skybox side of the fence rather than in ui/, for one
// concrete reason: everything under src/ui compiles into tweaker_ui, a static library shared with
// smoke_test, which has no D3D9 device and therefore no skybox module at all. This file is the
// adapter that is allowed to know about both - it registers itself with the menu through
// menu::add_extra_tab and calls into tw::skybox directly.
namespace tw::skybox::ui
{
// Registers the tab. Call once, after tw::ui::initialize() (which builds the menu) and before the
// first frame.
void initialize() noexcept;

// The menu tab index this module was assigned, or -1 before initialize(). sky_panel needs it to know
// whether the Skybox page is the one on screen.
[[nodiscard]] int tab_handle() noexcept;

// Once per frame, from ui_main::draw_frame, whether or not the menu is open: this is what turns a
// silent resolution fallback into a notefeed toast. Drawing the tab itself is driven by the menu.
void update() noexcept;

// The module's own overlay windows - today just the shader parameter panel (see sky_panel.hxx).
//
// Separate from update() because of where it has to sit in the frame: the panel docks to the menu's
// rectangle and must land above it, so it is drawn after menu::update(), while update() has to run
// before the notefeed so a toast raised this frame is shown this frame.
void draw_windows() noexcept;
} // namespace tw::skybox::ui
