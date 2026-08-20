#pragma once

#include "ui/overlay_state.hxx"
#include "ui/qp/qp_catalog.hxx"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// The UI-facing Quick Player model, synced from the host over the TW_OVL QP_* ops (see
// Docs/Internal/overlay-quickplayer.md). Exactly the same split as tw::ui::overlay_state: qp_wire
// parses the wire protocol on the IPC thread (inside the WM_COPYDATA WndProc handler) and calls the
// setters below; the UI/render thread never touches this state directly, it calls refresh() once
// per frame to pull a snapshot into its own cache.
namespace tw::ui::qp::state
{
// Field order is size-descending on purpose: tag_id is a uint8_t enum, so declaring it before the
// int padded this out to 12 bytes for no reason. Tracks live in vectors of these.
struct tag_state {
    int parameter = 0;
    tag_id id = tag_id::unknown;
    bool has_parameter = false;
};

struct track {
    std::string entry_id; // PlaylistEntry.Id, Guid "N" format
    std::string artist;
    std::string title;

    // character_id::none means "no per-track override" - the entry launches with the global
    // default, mirroring PlaylistEntry.Character being null.
    character_id character = character_id::none;

    std::vector<tag_state> tags;

    // The seven per-track asconfig overrides. Kept as a list of the ids that are on rather than a
    // bitmask so nothing here has to know how overlay_state packs its own array - there are at most
    // seven, and the only operations are "iterate them" and "is this one present".
    std::vector<overlay_state::tweak_id> mods;
};

struct playlist_summary {
    std::string playlist_id; // Playlist.Id, Guid "N" format
    std::string name;
    int track_count = 0;
};

using playlist_list = std::vector<playlist_summary>;
using track_list = std::vector<track>;

struct order_state {
    std::string playlist_id;
    playback_mode mode = playback_mode::sequential;
    advance_trigger advance = advance_trigger::automatic;

    // Empty when the module has no position in this playlist (see PlaybackController's
    // CurrentIndexIn - asking about a playlist other than the one being driven is a normal case).
    std::string current_entry_id;

    // What plays next, nearest first, capped host-side (QP_ORDER carries at most 25). Nothing draws
    // this yet - it is deliberately carried anyway, as groundwork for a custom song announcer, and
    // because it is what lets a Next/Prev request be confirmed against the exact entry the host is
    // about to start rather than against "any reply at all". See Docs/Internal/overlay-quickplayer.md.
    std::vector<std::string> upcoming;
};

// Reader side - a UI-owned snapshot, refreshed opportunistically once per frame via refresh().
// Never blocks the render thread.
//
// The two unbounded collections are held by shared_ptr to const rather than by value: a playlist
// can hold a thousand entries, and copying that (five strings and two vectors per track) on the
// render thread every time it changed would be a visible hitch. The writer publishes a fresh
// immutable vector and refresh() copies a pointer.
struct cache {
    std::shared_ptr<const playlist_list> playlists;

    // Which playlist `tracks` belongs to. Tracks are fetched lazily, one playlist at a time (see
    // QP_NOTIFY_SELECT), so this is not necessarily the playing one, nor the one `order` describes.
    std::string tracks_playlist_id;
    std::shared_ptr<const track_list> tracks;

    order_state order;

    // Empty entry id means nothing is playing. The playlist id is carried alongside because the
    // playing playlist need not be the one currently open in the overlay.
    std::string playing_playlist_id;
    std::string playing_entry_id;

    character_id default_character = character_id::mono;

    // Whether the host has ever pushed a catalog. Distinguishes "Quick Player has not talked to us"
    // from "the user genuinely has no playlists" - the two want different empty states.
    bool connected = false;

    std::uint32_t seen_generation = 0;
};

// Writer side - called only from the IPC thread, once per parsed op. Takes a short blocking lock;
// the only contention is refresh()'s try_lock, which never holds it longer than a few field copies.
void set_catalog(playlist_list playlists);
void set_tracks(std::string playlist_id, track_list tracks);
void set_order(order_state value);
void set_now_playing(std::string playlist_id, std::string entry_id);
void set_stopped();
void set_default_character(character_id id);
void reset();

// Call once per frame. Returns true only if `out` actually picked up newer data - false covers both
// "the writer is mid-update right now" and "nothing changed since the last successful refresh".
bool refresh(cache& out);

// Null-safe accessors for the two shared_ptr members: before the first push they are empty, and a
// null check at every draw site is noise. Both return a shared empty collection in that case.
[[nodiscard]] const playlist_list& playlists_of(const cache& snapshot) noexcept;
[[nodiscard]] const track_list& tracks_of(const cache& snapshot) noexcept;

// Nearest-first lookup by PlaylistEntry id within the currently loaded track list; null if the
// entry belongs to some other playlist or is simply gone.
[[nodiscard]] const track* find_track(const cache& snapshot, std::string_view entry_id) noexcept;

[[nodiscard]] bool has_mod(const track& entry, overlay_state::tweak_id id) noexcept;
[[nodiscard]] const tag_state* find_tag(const track& entry, tag_id id) noexcept;
} // namespace tw::ui::qp::state
