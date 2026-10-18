#pragma once

#include "lua/lua_script.hxx"

// The scripts as the host sees them: the SCRIPT_* half of TW_OVL, both directions
// (Docs/Internal/overlay-scripts.md; why it is shaped this way - script-manager.md §7).
//
// **Two threads, and a mailbox between them.** Commands arrive on whichever thread delivers WM_COPYDATA
// and are only parsed and posted there. Switching a script on is running its body, and the registry
// belongs to the engine/render thread - so the commands are carried out by pump(), once per overlay frame
// from lua_host::draw_frame, before any script draws: the same place an overlay click on the Scripts tab
// is carried out. An empty mailbox costs one atomic load.
//
// **What goes back is changes, not a feed.** pump() also compares registry::revision() with the one it
// last saw, and only when it moved looks at the scripts and sends a SCRIPT_STATE for each one whose state
// differs from what the host was last told. Every source of change - a host command, a click in the
// overlay, a failed load, a suspension in the scheduler, a Resume - only bumps the revision; none of them
// sends anything itself, so a suspension decided in the middle of a hooked channel call costs that call
// one increment.
//
// **The run order travels the same way** (protocol 3, script-coexistence.md §3): SCRIPT_ORDER from
// the host is lua_registry::set_order, and SCRIPT_ORDER back - the whole order - goes out with the full list,
// whenever the order differs from what the host was last sent, and always in answer to the host's, which is
// how the host knows its drag landed.
namespace tw::lua::remote
{
using send_fn = bool (*)(std::string_view op_line);
using present_fn = bool (*)() noexcept;

// The DLL registers tw::ipc::send_overlay_command and tw::ipc::host_present (ui_main.cxx). Without a
// host nothing is sent and nothing is remembered as sent: a connecting host gets the whole list anyway.
void set_link(send_fn send, present_fn present) noexcept;

// IPC thread. One already-split SCRIPT_* op: `op` is the leading token, `rest` the remainder, unparsed.
// Malformed and unknown ops are dropped silently, like every other op of the protocol.
void handle_op(std::string_view op, std::string_view rest);

// Any thread. Queues the whole list - every script's SCRIPT_STATE, then SCRIPT_SYNCED. Sent after the
// handshake: a host that just connected knows nothing.
void request_sync();

// Engine/render thread, once per overlay frame. Carries out what the mailbox holds, then tells the host
// what changed. Drops the mailbox unread while the VM is down - there is nothing to switch.
void pump() noexcept;

// What one script looks like on the wire, without the op: `<enabled> <status> [message]`, the message
// percent-encoded. Public for the harness.
[[nodiscard]] std::string describe(const script& entry);
} // namespace tw::lua::remote
