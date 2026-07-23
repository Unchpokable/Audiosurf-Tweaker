#pragma once

#include <string_view>

namespace tw::ipc
{
void initialize() noexcept;
void shutdown() noexcept;

// Sends a single already-formatted L3 op line (e.g. "NOTIFY_TWEAK InvisibleRoad true") to the
// bridge, wrapped in the TW_OVL envelope (see Docs/Internal/overlay-protocol.md, L2). Returns
// false if no bridge window is known yet (HANDSHAKE_BEGIN hasn't arrived) or the SendMessage
// itself failed - registered as tw::ui::pending_actions' send backend, see ui_main.cxx.
bool send_overlay_command(std::string_view op_line);
} // namespace tw::ipc
