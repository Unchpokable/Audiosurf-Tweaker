#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// Static Quick Player vocabularies, mirrored from the host rather than pushed over the wire.
//
// Deliberately hardcoded: unlike the skin catalog (which is whatever the user has on disk), these
// three lists are fixed by the game itself and the host already implements everything Audiosurf
// understands, so a QP_* op to carry them would cost a protocol operation to transmit a constant.
// The tradeoff is a sync obligation - see the table in Docs/Internal/overlay-quickplayer.md:
//
//   19 song tags -> QuickPlayerCore/Audiosurf/SongTagCatalog.cs
//   14 characters -> AudiosurfInterface/GameCharacter.cs + TweakerUI/Core/CharacterRoster.cs
//   6 modes / 2 advance triggers -> QuickPlayerCore/Playlist.cs
//
// The seven per-track "mods" deliberately have no mirror here at all: they are the same seven
// tweaks the overlay already knows, so qp code reuses overlay_state::tweak_* for them.
namespace tw::ui::qp
{
// Wire names are the C# enum member names (SongTagToken), not the bracket strings the game reads
// out of a file title - L3 speaks the user-facing meaning of a setting, the same convention the
// tweak wire names follow (see Docs/Internal/overlay-protocol.md).
enum class tag_id : std::uint8_t {
    four_lanes,
    portal,
    mono_only,
    everybody_mono,
    mono_less_grey,
    mono_all_grey,
    mono_no_grey,
    mono_base_points,
    no_stealth,
    sidewinder_camera,
    banking_camera,
    first_person,
    caterpillar,
    minimum_match_size,
    whites_blacks_percent,
    match_collection_ticks,
    puzzle_rows_count,
    hide_puzzle_grid,
    steep,
    unknown,
};

constexpr std::size_t k_tag_count = 19;

struct tag_definition {
    tag_id id = tag_id::unknown;
    std::string_view wire_name;      // "FourLanes"
    std::string_view label;          // "Play with 4 lanes" - SongTagDefinition::Description
    std::string_view bracket_prefix; // whole literal ("[as-4lane]") or the head of a parameterized one ("[as-msz")
    std::string_view bracket_suffix; // empty for literals, "]" for parameterized
    bool has_parameter = false;
    int min_parameter = 0;
    int max_parameter = 0;

    // Sidewinder/BankingCamera exist in SongTagCatalog but the host models them as *mods*
    // (ConfigOverrides), because they map to a live asconfig value rather than a title marker, and
    // TagOptionViewModel hides them from the Tags list for exactly that reason. A tag written into
    // PlaylistEntry.Tags instead would be silently dropped the first time the desktop edits that
    // track, so the overlay must not offer them here either - it shows them among the mods.
    bool selectable = true;
};

// All 19 definitions in SongTagCatalog order, including the two non-selectable ones: a hand-edited
// or imported playlist can still carry them in Tags, and dropping them from the parser would make
// the overlay silently disagree with what the host actually sent.
[[nodiscard]] std::span<const tag_definition> all_tags() noexcept;

[[nodiscard]] const tag_definition& tag(tag_id id) noexcept;
[[nodiscard]] tag_id resolve_tag_id(std::string_view wire_name) noexcept;

// The badge text the desktop shows for an applied tag - SongTagDefinition::Format's counterpart.
// A parameterized tag with no parameter renders its prefix only, matching how the desktop card
// skips a badge it cannot format yet rather than inventing a value.
[[nodiscard]] std::string format_tag(tag_id id, int parameter, bool has_parameter);

// Same 14 the transport bar's grid offers (CharacterRoster::RealCharacters) - GameCharacter's
// CurrentCharacter and Freeride members are deliberately absent, they are not player picks.
enum class character_id : std::uint8_t {
    none, // no per-track override; the entry launches with the global default
    mono,
    pointman,
    double_vision,
    mono_pro,
    vegas,
    eraser,
    pointman_pro,
    pusher,
    double_vision_pro,
    ninja_mono,
    eraser_elite,
    pointman_elite,
    pusher_elite,
    double_vision_elite,
};

constexpr std::size_t k_character_count = 14;

struct character_definition {
    character_id id = character_id::none;
    std::string_view wire_name;   // "DoubleVisionPro"
    std::string_view display_name; // "Double Vision Pro"
};

[[nodiscard]] std::span<const character_definition> all_characters() noexcept;
[[nodiscard]] std::string_view character_display_name(character_id id) noexcept;
[[nodiscard]] std::string_view character_wire_name(character_id id) noexcept;

// "-" on the wire (no override) resolves to character_id::none.
[[nodiscard]] character_id resolve_character_id(std::string_view wire_name) noexcept;

// QuickPlayerCore::PlaybackMode. Order matches the C# enum, but nothing depends on the numeric
// value: the wire carries names precisely so inserting a mode later cannot renumber anything.
enum class playback_mode : std::uint8_t {
    sequential,
    single,
    shuffle,
    repeat_one,
    repeat_all,
    shuffle_loop,
};

constexpr std::size_t k_playback_mode_count = 6;

struct playback_mode_definition {
    playback_mode id = playback_mode::sequential;
    std::string_view wire_name;    // "ShuffleLoop"
    std::string_view display_name; // "Shuffle loop" - what the desktop's tooltip leads with
    std::string_view hint;

    // ui/image/svg key. The assets are ports of the desktop's own IconQPMode* geometries, so the
    // two surfaces show the same glyph family for the same six modes.
    std::string_view icon_key;
};

[[nodiscard]] std::span<const playback_mode_definition> all_playback_modes() noexcept;
[[nodiscard]] playback_mode resolve_playback_mode(std::string_view wire_name) noexcept;
[[nodiscard]] std::string_view playback_mode_wire_name(playback_mode mode) noexcept;
[[nodiscard]] std::string_view playback_mode_display_name(playback_mode mode) noexcept;

// QuickPlayerCore::AdvanceTrigger, carried as auto/manual rather than the C# member names: the
// desktop labels these Auto/Manual too, and SongComplete/CharacterScreen only mean anything if you
// already know which game report each names.
enum class advance_trigger : std::uint8_t {
    automatic, // AdvanceTrigger.SongComplete
    manual,    // AdvanceTrigger.CharacterScreen
};

[[nodiscard]] advance_trigger resolve_advance_trigger(std::string_view token) noexcept;
[[nodiscard]] std::string_view advance_trigger_wire_name(advance_trigger trigger) noexcept;
} // namespace tw::ui::qp
