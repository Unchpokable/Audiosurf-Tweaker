// No TweakerPlugin PCH here - shared with smoke_test, same convention as pending_actions.cxx.
#include "ui/qp/qp_pending.hxx"

#include "ui/plugins/static/notefeed.hxx"
#include "ui/qp/qp_wire.hxx"

#include <chrono>
#include <string>
#include <vector>

namespace
{
namespace qp = tw::ui::qp;
namespace state = tw::ui::qp::state;

using my_clock_t = std::chrono::steady_clock;

// Same window pending_actions gives a tweak: long enough for a WM_COPYDATA round trip through
// asbridge on a loaded machine, short enough that a dead host is obvious rather than just sluggish.
constexpr auto k_confirm_timeout = std::chrono::milliseconds { 5000 };

// One in-flight request of a kind that has exactly one slot. The `active` flag rather than an
// optional so clearing never reallocates the string members.
struct slot {
    my_clock_t::time_point deadline {};
    bool active = false;

    void arm()
    {
        deadline = my_clock_t::now() + k_confirm_timeout;
        active = true;
    }

    // Confirmed, expired, or still waiting. Confirmation is silent - the UI already showed the
    // desired value, so there is nothing to announce.
    enum class outcome : std::uint8_t { waiting, confirmed, expired };

    outcome poll(bool satisfied, my_clock_t::time_point now)
    {
        if(!active) {
            return outcome::waiting;
        }

        if(satisfied) {
            active = false;
            return outcome::confirmed;
        }

        if(now >= deadline) {
            active = false;
            return outcome::expired;
        }

        return outcome::waiting;
    }
};

struct mode_request : slot {
    std::string playlist_id;
    qp::playback_mode mode = qp::playback_mode::sequential;
};

struct advance_request : slot {
    std::string playlist_id;
    qp::advance_trigger trigger = qp::advance_trigger::automatic;
};

struct character_request : slot {
    qp::character_id id = qp::character_id::mono;
};

struct transport_request : slot {
    std::string action;           // "play", "stop", "next", "prev"
    std::string expected_entry_id; // for "play"; empty otherwise
    std::string previous_entry_id; // what was playing when the request went out
};

// Per-track edits are keyed rather than slotted: toggling two tags on one track, or one tag on two
// tracks, are independent requests and must not evict each other.
enum class edit_kind : std::uint8_t { tag, mod, character };

struct track_edit : slot {
    edit_kind kind = edit_kind::tag;
    std::string entry_id;
    std::string label; // what the failure toast names

    qp::tag_id tag = qp::tag_id::unknown;
    tw::ui::overlay_state::tweak_id mod = tw::ui::overlay_state::tweak_id::unknown;
    qp::character_id character = qp::character_id::none;

    bool enabled = false;
    int parameter = 0;
    bool has_parameter = false;
};

mode_request g_mode;
advance_request g_advance;
character_request g_character;
transport_request g_transport;
std::vector<track_edit> g_edits;

// Replaces any pending edit addressing the same thing, so a double toggle is one request rather
// than two racing ones.
track_edit& claim_edit(edit_kind kind, std::string_view entry_id, qp::tag_id tag, tw::ui::overlay_state::tweak_id mod)
{
    for(track_edit& edit : g_edits) {
        const bool same_target = edit.kind == kind && edit.entry_id == entry_id
                                 && (kind != edit_kind::tag || edit.tag == tag) && (kind != edit_kind::mod || edit.mod == mod);
        if(same_target) {
            return edit;
        }
    }

    // Reuse a spent entry before growing - the vector is otherwise a slow leak across a long
    // session of toggling things.
    for(track_edit& edit : g_edits) {
        if(!edit.active) {
            edit = track_edit {};
            return edit;
        }
    }

    g_edits.emplace_back();
    return g_edits.back();
}

const track_edit* find_edit(edit_kind kind, std::string_view entry_id, qp::tag_id tag, tw::ui::overlay_state::tweak_id mod) noexcept
{
    for(const track_edit& edit : g_edits) {
        if(!edit.active || edit.kind != kind || edit.entry_id != entry_id) {
            continue;
        }
        if(kind == edit_kind::tag && edit.tag != tag) {
            continue;
        }
        if(kind == edit_kind::mod && edit.mod != mod) {
            continue;
        }
        return &edit;
    }

    return nullptr;
}

void push_failure(const std::string& text)
{
    tw::ui::plugins::statics::notefeed::push(text);
}

// True once the snapshot shows the edit's desired state, i.e. the host applied it and echoed a
// fresh QP_TRACKS. An entry that vanished from the loaded list counts as satisfied: it was removed
// or the user opened another playlist, and either way there is nothing left to wait for.
bool edit_satisfied(const track_edit& edit, const state::cache& snapshot)
{
    const state::track* entry = state::find_track(snapshot, edit.entry_id);
    if(entry == nullptr) {
        return true;
    }

    switch(edit.kind) {
        case edit_kind::tag: {
            const state::tag_state* tag = state::find_tag(*entry, edit.tag);
            if(!edit.enabled) {
                return tag == nullptr;
            }
            return tag != nullptr && (!edit.has_parameter || (tag->has_parameter && tag->parameter == edit.parameter));
        }
        case edit_kind::mod:
            return state::has_mod(*entry, edit.mod) == edit.enabled;
        case edit_kind::character:
            return entry->character == edit.character;
    }

    return true;
}
} // namespace

namespace tw::ui::qp::pending
{
void request_mode(std::string_view playlist_id, playback_mode mode)
{
    g_mode.playlist_id = playlist_id;
    g_mode.mode = mode;
    g_mode.arm();
    send_mode(playlist_id, mode);
}

void request_advance(std::string_view playlist_id, advance_trigger trigger)
{
    g_advance.playlist_id = playlist_id;
    g_advance.trigger = trigger;
    g_advance.arm();
    send_advance(playlist_id, trigger);
}

void request_default_character(character_id id)
{
    g_character.id = id;
    g_character.arm();
    send_default_character(id);
}

void request_play(std::string_view playlist_id, std::string_view entry_id)
{
    g_transport.action = "play";
    g_transport.expected_entry_id = entry_id;
    g_transport.previous_entry_id.clear();
    g_transport.arm();
    send_play(playlist_id, entry_id);
}

void request_transport(std::string_view action, std::string_view playing_entry_id)
{
    g_transport.action = action;
    g_transport.expected_entry_id.clear();
    g_transport.previous_entry_id = playing_entry_id;
    g_transport.arm();
    send_transport(action);
}

void request_tag(std::string_view entry_id, tag_id id, bool enabled, int parameter, bool has_parameter)
{
    track_edit& edit = claim_edit(edit_kind::tag, entry_id, id, overlay_state::tweak_id::unknown);
    edit.kind = edit_kind::tag;
    edit.entry_id = entry_id;
    edit.tag = id;
    edit.enabled = enabled;
    edit.parameter = parameter;
    edit.has_parameter = has_parameter;
    edit.label = tag(id).label;
    edit.arm();
    send_tag(entry_id, id, enabled, parameter, has_parameter);
}

void request_mod(std::string_view entry_id, overlay_state::tweak_id id, bool enabled)
{
    track_edit& edit = claim_edit(edit_kind::mod, entry_id, tag_id::unknown, id);
    edit.kind = edit_kind::mod;
    edit.entry_id = entry_id;
    edit.mod = id;
    edit.enabled = enabled;
    edit.label = overlay_state::tweak_display_name(id);
    edit.arm();
    send_mod(entry_id, id, enabled);
}

void request_character(std::string_view entry_id, character_id id)
{
    track_edit& edit = claim_edit(edit_kind::character, entry_id, tag_id::unknown, overlay_state::tweak_id::unknown);
    edit.kind = edit_kind::character;
    edit.entry_id = entry_id;
    edit.character = id;
    edit.label = id == character_id::none ? "character override" : std::string(character_display_name(id));
    edit.arm();
    send_character(entry_id, id);
}

void update(const state::cache& snapshot)
{
    const auto now = my_clock_t::now();

    const bool mode_ok = snapshot.order.playlist_id == g_mode.playlist_id && snapshot.order.mode == g_mode.mode;
    if(g_mode.poll(mode_ok, now) == slot::outcome::expired) {
        push_failure("Failed to change playback mode");
    }

    const bool advance_ok = snapshot.order.playlist_id == g_advance.playlist_id && snapshot.order.advance == g_advance.trigger;
    if(g_advance.poll(advance_ok, now) == slot::outcome::expired) {
        push_failure("Failed to change advance mode");
    }

    if(g_character.poll(snapshot.default_character == g_character.id, now) == slot::outcome::expired) {
        push_failure("Failed to change the default character");
    }

    // "play" knows the entry it asked for; "stop" wants silence; "next"/"prev" only know that
    // whatever was playing should not be any more (see request_transport).
    bool transport_ok = false;
    if(g_transport.action == "play") {
        transport_ok = snapshot.playing_entry_id == g_transport.expected_entry_id;
    }
    else if(g_transport.action == "stop") {
        transport_ok = snapshot.playing_entry_id.empty();
    }
    else {
        transport_ok = snapshot.playing_entry_id != g_transport.previous_entry_id;
    }

    if(g_transport.poll(transport_ok, now) == slot::outcome::expired) {
        push_failure(g_transport.action == "stop" ? "Failed to stop playback" : "Failed to start the track");
    }

    for(track_edit& edit : g_edits) {
        if(edit.poll(edit_satisfied(edit, snapshot), now) == slot::outcome::expired) {
            push_failure("Failed to change " + edit.label);
        }
    }
}

playback_mode display_mode(const state::cache& snapshot) noexcept
{
    if(g_mode.active && g_mode.playlist_id == snapshot.order.playlist_id) {
        return g_mode.mode;
    }

    return snapshot.order.mode;
}

advance_trigger display_advance(const state::cache& snapshot) noexcept
{
    if(g_advance.active && g_advance.playlist_id == snapshot.order.playlist_id) {
        return g_advance.trigger;
    }

    return snapshot.order.advance;
}

character_id display_default_character(const state::cache& snapshot) noexcept
{
    return g_character.active ? g_character.id : snapshot.default_character;
}

std::string_view display_playing_entry(const state::cache& snapshot) noexcept
{
    // Only "play" has an entry to show optimistically. A next/prev in flight keeps showing the old
    // row rather than guessing which one the host will land on - a marker that jumped to the wrong
    // track and then jumped again would read as a bug.
    if(g_transport.active && g_transport.action == "play") {
        return g_transport.expected_entry_id;
    }

    return snapshot.playing_entry_id;
}

bool display_tag_enabled(const state::track& entry, tag_id id) noexcept
{
    if(const track_edit* edit = find_edit(edit_kind::tag, entry.entry_id, id, overlay_state::tweak_id::unknown)) {
        return edit->enabled;
    }

    return state::find_tag(entry, id) != nullptr;
}

int display_tag_parameter(const state::track& entry, tag_id id) noexcept
{
    if(const track_edit* edit = find_edit(edit_kind::tag, entry.entry_id, id, overlay_state::tweak_id::unknown)) {
        return edit->parameter;
    }

    const state::tag_state* tag = state::find_tag(entry, id);
    return tag != nullptr ? tag->parameter : 0;
}

bool display_mod_enabled(const state::track& entry, overlay_state::tweak_id id) noexcept
{
    if(const track_edit* edit = find_edit(edit_kind::mod, entry.entry_id, tag_id::unknown, id)) {
        return edit->enabled;
    }

    return state::has_mod(entry, id);
}

character_id display_character(const state::track& entry) noexcept
{
    if(const track_edit* edit = find_edit(edit_kind::character, entry.entry_id, tag_id::unknown, overlay_state::tweak_id::unknown)) {
        return edit->character;
    }

    return entry.character;
}

bool has_pending_transport() noexcept
{
    return g_transport.active;
}

void reset() noexcept
{
    g_mode = {};
    g_advance = {};
    g_character = {};
    g_transport = {};
    g_edits.clear();
}
} // namespace tw::ui::qp::pending
