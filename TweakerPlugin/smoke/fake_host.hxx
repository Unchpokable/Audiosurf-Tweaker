#pragma once

#include <string_view>

// The pretend host both smoke modes run against: overlay_state seeded through its public setters, and
// a miniature Quick Player host that speaks real QP_* payloads to the real parser. Shared by the
// interactive window (main.cxx) and the snapshot stand (shots.cxx), which must show the same data.
namespace tw::smoke
{
// Fills overlay_state with plausible skins and tweaks, as if the host had just handshaken.
void seed_fake_overlay_state();

// Builds the fake playlists and pushes them through qp::handle_op, the way the host would.
void seed_fake_quick_player();

// pending_actions' and qp's send backend: answers requests the way the real host does.
bool send_overlay_command(std::string_view op_line);

// When off, NOTIFY_TWEAK/NOTIFY_SKIN are swallowed so pending_actions' timeout path can be watched.
// Quick Player requests are always answered.
bool& auto_confirm() noexcept;
} // namespace tw::smoke
