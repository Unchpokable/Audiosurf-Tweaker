#pragma once

#include <string_view>

namespace tw::ipc
{
void initialize() noexcept;
void shutdown() noexcept;

// Stage 3 of startup (plugin/load.cxx). Once per process; a second call does nothing. The thread wakes once a
// second and, only while a host is connected, checks that its bridge window still exists - the one way to
// notice a host that died without sending HOST_DISCONNECT (Docs/Internal/plugin-offline-mode.md §4.5).
void start_host_watchdog() noexcept;

// Whether Audiosurf Tweaker is connected. A relaxed atomic read, safe from any thread. The render thread
// does not need it: its snapshot carries the same fact (overlay_state::cache::host_connected), consistent
// with the rest of the host's state.
[[nodiscard]] bool host_present() noexcept;

// Sends a single already-formatted L3 op line (e.g. "NOTIFY_TWEAK InvisibleRoad true") to the
// bridge, wrapped in the TW_OVL envelope (see Docs/Internal/overlay-protocol.md, L2). Returns
// false if no host is connected or the SendMessage itself failed - in which case, if the bridge
// window is gone, the plugin goes offline on the spot. Registered as tw::ui::pending_actions' send
// backend, see ui_main.cxx.
bool send_overlay_command(std::string_view op_line);
} // namespace tw::ipc
