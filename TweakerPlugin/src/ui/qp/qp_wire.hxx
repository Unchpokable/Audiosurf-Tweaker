#pragma once

#include "ui/overlay_state.hxx"
#include "ui/qp/qp_catalog.hxx"

#include <string_view>

// The QP_* half of the L3 overlay protocol (Docs/Internal/overlay-quickplayer.md), both directions.
//
// ipc/overlay_ipc deliberately does not parse these itself: it recognises the QP_ prefix and hands
// the payload over whole, so the entire Quick Player grammar lives in one translation unit instead
// of growing the transport layer by a dozen ops. Everything parsed here lands in tw::ui::qp::state.
namespace tw::ui::qp
{
// Applies one already-split QP_* op. `op` is the leading token ("QP_TRACKS"), `rest` is everything
// after it, unparsed. Malformed or unknown ops are ignored silently, exactly like the tweak/skin
// ops - a host that speaks a newer protocol must not be able to break an older overlay.
//
// Called synchronously on whichever thread delivered the WM_COPYDATA (see "Многопоточность" in
// Docs/Internal/overlay-protocol.md); it only parses and writes to qp::state, never draws.
void handle_op(std::string_view op, std::string_view rest);

// Outbound side. Same pluggable-backend pattern as ui/pending_actions: the DLL registers
// tw::ipc::send_overlay_command, smoke_test registers its own stub, and this module stays free of
// any IPC dependency so it can live in tweaker_ui alongside the UI it serves.
using send_fn = bool (*)(std::string_view op_line);

void set_send_backend(send_fn fn) noexcept;

// Asks the host to open a playlist - the trigger for the lazy QP_TRACKS push, and the one request
// that is a fetch rather than a mutation (nothing to roll back if it never answers).
bool send_select(std::string_view playlist_id);

bool send_play(std::string_view playlist_id, std::string_view entry_id);

// action is "stop", "next" or "prev".
bool send_transport(std::string_view action);

bool send_mode(std::string_view playlist_id, playback_mode mode);
bool send_advance(std::string_view playlist_id, advance_trigger trigger);

bool send_default_character(character_id id);

// Per-track edits. character_id::none clears the override, matching PlaylistEntry.Character = null.
bool send_tag(std::string_view entry_id, tag_id id, bool enabled, int parameter, bool has_parameter);
bool send_mod(std::string_view entry_id, overlay_state::tweak_id id, bool enabled);
bool send_character(std::string_view entry_id, character_id id);
} // namespace tw::ui::qp
