#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Owns the UI-facing model synced from the host over TW_OVL (skin catalog, current skin, tweak
// on/off state). This is UI/domain data, not IPC transport state: overlay_ipc parses the wire
// protocol on the IPC thread (inside the WM_COPYDATA WndProc handler) and calls the setters below;
// the UI/render thread never touches this state directly - it calls refresh() once per frame to
// pull a snapshot into its own tw::ui::overlay_state::cache. No IPC/rendering dependency lives
// here.
namespace tw::ui::overlay_state
{
// Mirrors the 7 tweaks the game/host actually track (TweakerUI/ViewModels/TweakerViewModel.cs,
// TweakerUI/Models/ModOptionViewModel.cs): the 4 "classic" ones plus the 3 Freeride tweaks.
enum class tweak_id : std::uint8_t {
    invisible_road,
    hidden_song_title,
    sidewinder_camera,
    banking_camera,
    freeride_no_blocks,
    freeride_blocks_caterpillars,
    freeride_auto_advance_disable,
    unknown,
};

constexpr std::size_t k_tweak_count = 7;

// Maps a TW_OVL wire token (e.g. "InvisibleRoad") to its tweak_id, or tweak_id::unknown.
tweak_id resolve_tweak_id(std::string_view wire_name) noexcept;

// Human-readable label for display purposes only (pins, menu Tweaks tab) - not part of the wire
// protocol. Empty for tweak_id::unknown.
[[nodiscard]] std::string_view tweak_display_name(tweak_id id) noexcept;

// Inverse of resolve_tweak_id() - the TW_OVL wire token for a known tweak_id (e.g.
// "InvisibleRoad"). Used by pending_actions to build outbound NOTIFY_TWEAK ops. Empty for
// tweak_id::unknown.
[[nodiscard]] std::string_view tweak_wire_name(tweak_id id) noexcept;

// All 7 known tweak ids, in the same order as k_tweak_count/tweak_index - convenient for widgets
// that need to iterate every tweak (menu Tweaks tab, pins).
[[nodiscard]] const std::array<tweak_id, k_tweak_count>& all_tweak_ids() noexcept;

// Writer side - called only from the IPC thread (inside the WM_COPYDATA WndProc handler), once
// per parsed op. Takes a short blocking lock; the only contention is against refresh()'s
// try_lock() below, which never holds it longer than a few field copies, so any wait here is
// negligible.
void set_tweak(tweak_id id, bool enabled, bool quick_player = false);
void set_skin_list(std::vector<std::string> names);
void set_current_skin(std::string name);
void reset();

// Reader side - a UI-owned snapshot, refreshed opportunistically once per frame via refresh().
// Never blocks the render thread: on lock contention (the writer is mid-update) refresh() just
// leaves the previous frame's snapshot untouched and returns false.
struct cache {
    std::array<std::uint8_t, k_tweak_count> tweak_enabled {};
    std::array<std::uint8_t, k_tweak_count> tweak_quick_player {};
    std::vector<std::string> skin_names;
    std::string current_skin_name;
    std::uint32_t seen_generation = 0;
};

// Call once per frame. Returns true only if `out` actually picked up newer data - false covers
// both "the writer is mid-update right now" and "nothing changed since the last successful
// refresh" (indistinguishable to the caller, and both simply mean "keep using the existing
// cache"). On success, no more work is done than a handful of field copies - this never happens
// on frames where nothing changed, even if the lock itself was uncontended.
bool refresh(cache& out);

bool is_tweak_enabled(const cache& snapshot, tweak_id id) noexcept;
bool is_tweak_quick_player(const cache& snapshot, tweak_id id) noexcept;
} // namespace tw::ui::overlay_state
