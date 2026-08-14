// No TweakerPlugin PCH here - this TU is shared with smoke_test (see src/ui/CMakeLists.txt) and
// has no D3D9/Quest3D dependency, only STL. Same convention as overlay_state.cxx/pending_actions.cxx.
#include "ui/qp/qp_catalog.hxx"

namespace
{
using tw::ui::qp::advance_trigger;
using tw::ui::qp::character_definition;
using tw::ui::qp::character_id;
using tw::ui::qp::playback_mode;
using tw::ui::qp::playback_mode_definition;
using tw::ui::qp::tag_definition;
using tw::ui::qp::tag_id;

// Two SongTagCatalog entries declare no upper bound at all (int.MaxValue): MonoBasePoints and
// MatchCollectionTicks. This is the overlay's own practical ceiling for the numeric field, not a
// host constraint - the game's own defaults for those two are 3 and 10-25, so nothing reachable is
// being cut off, and an unbounded spinner is worse to drive with a mouse than a bounded one.
constexpr int k_unbounded_parameter = 9999;

// Mirrors SongTagCatalog.All, in the same order. Bracket text is split into prefix/suffix rather
// than kept as a format string because that is the only shape any of the 19 actually take: a
// literal, or "<head><number>]".
constexpr tag_definition k_tags[] = {
    { tag_id::four_lanes, "FourLanes", "Play with 4 lanes", "[as-4lane]", "", false, 0, 0, true },
    { tag_id::portal, "Portal", "Portal Mode", "[as-portal]", "", false, 0, 0, true },
    { tag_id::mono_only, "MonoOnly", "Mono characters required", "[as-monoonly]", "", false, 0, 0, true },
    { tag_id::everybody_mono, "EverybodyMono", "All characters have only 2 block colors", "[as-everybodymono]", "", false, 0, 0, true },
    { tag_id::mono_less_grey, "MonoLessGrey", "Mono characters have less grey blocks", "[as-lessgrey]", "", false, 0, 0, true },
    { tag_id::mono_all_grey,
        "MonoAllGrey",
        "Mono all greys - scores start at 1000 and go down with each hit",
        "[as-allgrey]",
        "",
        false,
        0,
        0,
        true },
    { tag_id::mono_no_grey, "MonoNoGrey", "Mono characters have no grey blocks", "[as-nogrey]", "", false, 0, 0, true },
    { tag_id::mono_base_points,
        "MonoBasePoints",
        "Base points for Mono characters (default 3)",
        "[as-monopt",
        "]",
        true,
        0,
        k_unbounded_parameter,
        true },
    { tag_id::no_stealth, "NoStealth", "Remove Stealth and cumulative hit bonuses", "[as-nostlth]", "", false, 0, 0, true },

    // selectable = false: modelled as mods on the host, see tag_definition::selectable.
    { tag_id::sidewinder_camera, "SidewinderCamera", "Sidewinder camera mode", "[as-swind]", "", false, 0, 0, false },
    { tag_id::banking_camera, "BankingCamera", "Banking camera mode", "[as-bankcam]", "", false, 0, 0, false },

    { tag_id::first_person, "FirstPerson", "First-person camera mode", "[as-first]", "", false, 0, 0, true },
    { tag_id::caterpillar, "Caterpillar", "Big strings of blocks instead of only solo blocks", "[as-caterp]", "", false, 0, 0, true },
    { tag_id::minimum_match_size, "MinimumMatchSize", "Minimum match size", "[as-msz", "]", true, 0, 24, true },
    { tag_id::whites_blacks_percent, "WhitesBlacksPercent", "Percentage of white/black blocks", "[as-wb", "]", true, 0, 100, true },
    { tag_id::match_collection_ticks,
        "MatchCollectionTicks",
        "Match collection ticks (default 10-25)",
        "[as-mt",
        "]",
        true,
        0,
        k_unbounded_parameter,
        true },
    { tag_id::puzzle_rows_count, "PuzzleRowsCount", "Puzzle row count", "[as-prows", "]", true, 0, 8, true },
    { tag_id::hide_puzzle_grid, "HidePuzzleGrid", "Play with an invisible puzzle grid", "[as-hidepuz]", "", false, 0, 0, true },
    { tag_id::steep,
        "Steep",
        "For slow songs, dramatically increase slope and speed of the track",
        "[as-steep]",
        "",
        false,
        0,
        0,
        true },
};

static_assert(std::size(k_tags) == tw::ui::qp::k_tag_count);

constexpr tag_definition k_unknown_tag {};

constexpr character_definition k_characters[] = {
    { character_id::mono, "Mono", "Mono" },
    { character_id::pointman, "Pointman", "Pointman" },
    { character_id::double_vision, "DoubleVision", "Double Vision" },
    { character_id::mono_pro, "MonoPro", "Mono Pro" },
    { character_id::vegas, "Vegas", "Vegas" },
    { character_id::eraser, "Eraser", "Eraser" },
    { character_id::pointman_pro, "PointmanPro", "Pointman Pro" },
    { character_id::pusher, "Pusher", "Pusher" },
    { character_id::double_vision_pro, "DoubleVisionPro", "Double Vision Pro" },
    { character_id::ninja_mono, "NinjaMono", "Ninja Mono" },
    { character_id::eraser_elite, "EraserElite", "Eraser Elite" },
    { character_id::pointman_elite, "PointmanElite", "Pointman Elite" },
    { character_id::pusher_elite, "PusherElite", "Pusher Elite" },
    { character_id::double_vision_elite, "DoubleVisionElite", "Double Vision Elite" },
};

static_assert(std::size(k_characters) == tw::ui::qp::k_character_count);

// Labels and hints match the desktop chips (TweakerUI/Models/PlaybackModeOptionViewModel.cs) so the
// two surfaces read as one setting, and are ordered the same way: least to most persistent.
constexpr playback_mode_definition k_modes[] = {
    { playback_mode::single, "Single", "Single", "Play the started track and stop.", "icons/mode_single.svg" },
    { playback_mode::sequential,
        "Sequential",
        "In order",
        "Playlist order, stopping after the last track.",
        "icons/mode_sequential.svg" },
    { playback_mode::repeat_one,
        "RepeatOne",
        "Repeat one",
        "Restart the same track every time it finishes.",
        "icons/mode_repeat_one.svg" },
    { playback_mode::repeat_all, "RepeatAll", "Repeat all", "Playlist order, wrapping around forever.", "icons/mode_repeat_all.svg" },
    { playback_mode::shuffle,
        "Shuffle",
        "Shuffle",
        "Shuffled order, one pass over the playlist, then stop.",
        "icons/mode_shuffle.svg" },
    { playback_mode::shuffle_loop,
        "ShuffleLoop",
        "Shuffle loop",
        "Shuffled order, reshuffled into a fresh pass every time one runs out.",
        "icons/mode_shuffle_loop.svg" },
};

static_assert(std::size(k_modes) == tw::ui::qp::k_playback_mode_count);
} // namespace

namespace tw::ui::qp
{
std::span<const tag_definition> all_tags() noexcept
{
    return std::span { k_tags };
}

const tag_definition& tag(tag_id id) noexcept
{
    for(const tag_definition& definition : k_tags) {
        if(definition.id == id) {
            return definition;
        }
    }

    return k_unknown_tag;
}

tag_id resolve_tag_id(std::string_view wire_name) noexcept
{
    for(const tag_definition& definition : k_tags) {
        if(definition.wire_name == wire_name) {
            return definition.id;
        }
    }

    return tag_id::unknown;
}

std::string format_tag(tag_id id, int parameter, bool has_parameter)
{
    const tag_definition& definition = tag(id);

    std::string text { definition.bracket_prefix };
    if(definition.has_parameter && has_parameter) {
        text += std::to_string(parameter);
        text.append(definition.bracket_suffix);
    }

    return text;
}

std::span<const character_definition> all_characters() noexcept
{
    return std::span { k_characters };
}

std::string_view character_display_name(character_id id) noexcept
{
    for(const character_definition& definition : k_characters) {
        if(definition.id == id) {
            return definition.display_name;
        }
    }

    return {};
}

std::string_view character_wire_name(character_id id) noexcept
{
    for(const character_definition& definition : k_characters) {
        if(definition.id == id) {
            return definition.wire_name;
        }
    }

    return {};
}

character_id resolve_character_id(std::string_view wire_name) noexcept
{
    for(const character_definition& definition : k_characters) {
        if(definition.wire_name == wire_name) {
            return definition.id;
        }
    }

    return character_id::none;
}

std::span<const playback_mode_definition> all_playback_modes() noexcept
{
    return std::span { k_modes };
}

playback_mode resolve_playback_mode(std::string_view wire_name) noexcept
{
    for(const playback_mode_definition& definition : k_modes) {
        if(definition.wire_name == wire_name) {
            return definition.id;
        }
    }

    // Sequential is what an absent Mode deserializes to on the host, so an unrecognised token
    // lands on the same behaviour a playlist saved before modes existed would have had.
    return playback_mode::sequential;
}

std::string_view playback_mode_wire_name(playback_mode mode) noexcept
{
    for(const playback_mode_definition& definition : k_modes) {
        if(definition.id == mode) {
            return definition.wire_name;
        }
    }

    return {};
}

std::string_view playback_mode_display_name(playback_mode mode) noexcept
{
    for(const playback_mode_definition& definition : k_modes) {
        if(definition.id == mode) {
            return definition.display_name;
        }
    }

    return {};
}

advance_trigger resolve_advance_trigger(std::string_view token) noexcept
{
    return token == "manual" ? advance_trigger::manual : advance_trigger::automatic;
}

std::string_view advance_trigger_wire_name(advance_trigger trigger) noexcept
{
    return trigger == advance_trigger::manual ? "manual" : "auto";
}
} // namespace tw::ui::qp
