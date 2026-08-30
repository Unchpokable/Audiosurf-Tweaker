#pragma once

// The overlay's Scripts tab: what is installed, what is running, and a switch for each.
//
// Lives here rather than in tweaker_ui for the same reason the Skybox tab does - it talks to
// tw::lua::host, which owns a LuaJIT VM and the game's channel graph, neither of which exists in
// smoke_test. It registers itself through menu::add_extra_tab.
namespace tw::lua::ui
{
// Must run after tw::ui::initialize() (which creates the menu) and before the first frame (which
// builds the tab strip once and never rebuilds it).
void initialize() noexcept;
} // namespace tw::lua::ui
