// The fake host for smoke_test - see fake_host.hxx. Moved out of main.cxx unchanged when the snapshot
// stand (shots.cxx) became its second user.
#include "fake_host.hxx"

#include <cstdio>
#include <string>
#include <vector>

#include "ui/overlay_state.hxx"
#include "ui/qp/qp_wire.hxx"
#include "ui/wire_text.hxx"

namespace
{
// Toggled from the smoke controls window - see draw_smoke_controls(). Lets both pending_actions
// branches (Docs/Internal/overlay-protocol.md, "Reverse-sync") be eyeballed without a real host:
// on, requests "round-trip" instantly; off, they're swallowed and pending_actions' own timeout +
// notefeed failure toast fires ~5s later.
bool g_smoke_auto_confirm = true;

// Mirrors overlay_ipc.cxx's percent_decode - smoke has no ipc/ dependency of its own, so this is a
// small local copy rather than exposing that file-local helper across TUs for one call site.
std::string smoke_percent_decode(std::string_view s)
{
    const auto from_hex = [](char c) -> int {
        if(c >= '0' && c <= '9') {
            return c - '0';
        }
        if(c >= 'a' && c <= 'f') {
            return 10 + c - 'a';
        }
        if(c >= 'A' && c <= 'F') {
            return 10 + c - 'A';
        }
        return -1;
    };

    std::string out;
    out.reserve(s.size());
    for(std::size_t i = 0; i < s.size();) {
        if(s[i] == '%' && i + 2 < s.size()) {
            const int hi = from_hex(s[i + 1]);
            const int lo = from_hex(s[i + 2]);
            if(hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 3;
                continue;
            }
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

// ---------------------------------------------------------------------------------------------
// A miniature Quick Player host.
//
// The plugin's QP_* state is only reachable through the wire protocol, so rather than poking
// qp::state's setters directly (as the overlay_state seeding above does), this builds real QP_*
// payloads and feeds them to the real parser. That makes the smoke harness exercise the whole
// chain - serialization, parsing, state, UI - and gives the Player tab something to show without a
// running game. See Docs/Internal/overlay-quickplayer.md for the op formats.
// ---------------------------------------------------------------------------------------------
struct smoke_tag {
    std::string wire_name;
    std::string parameter = "-";
};

struct smoke_track {
    std::string id;
    std::string artist;
    std::string title;
    std::string character = "-";
    std::vector<smoke_tag> tags;
    std::vector<std::string> mods;
};

struct smoke_playlist {
    std::string id;
    std::string name;
    std::string mode = "Sequential";
    std::string advance = "auto";
    std::vector<smoke_track> tracks;
};

std::vector<smoke_playlist> g_smoke_playlists;
std::string g_smoke_open_id;
std::string g_smoke_playing_id;
std::string g_smoke_default_character = "Mono";

void smoke_feed(const std::string& payload)
{
    const auto space = payload.find(' ');
    const std::string_view op = space == std::string::npos ? std::string_view { payload } : std::string_view { payload }.substr(0, space);
    const std::string_view rest = space == std::string::npos ? std::string_view {} : std::string_view { payload }.substr(space + 1);
    tw::ui::qp::handle_op(op, rest);
}

// Matches QuickPlayerOverlayBridge.Text(): an empty token cannot exist on the wire, so empty text
// becomes an encoded space rather than nothing at all.
std::string smoke_text(const std::string& value)
{
    const std::string encoded = tw::ui::wire::percent_encode(value);
    return encoded.empty() ? "%20" : encoded;
}

smoke_playlist* smoke_find_playlist(std::string_view id)
{
    for(smoke_playlist& playlist : g_smoke_playlists) {
        if(playlist.id == id) {
            return &playlist;
        }
    }
    return nullptr;
}

void smoke_push_catalog()
{
    std::string payload = "QP_CATALOG " + std::to_string(g_smoke_playlists.size());
    for(const smoke_playlist& playlist : g_smoke_playlists) {
        payload += ' ' + playlist.id + ' ' + smoke_text(playlist.name) + ' ' + std::to_string(playlist.tracks.size());
    }
    smoke_feed(payload);
}

void smoke_push_tracks(std::string_view playlist_id)
{
    const smoke_playlist* playlist = smoke_find_playlist(playlist_id);
    if(playlist == nullptr) {
        return;
    }

    std::string payload = "QP_TRACKS " + playlist->id + ' ' + std::to_string(playlist->tracks.size());
    for(const smoke_track& track : playlist->tracks) {
        payload += ' ' + track.id + ' ' + smoke_text(track.artist) + ' ' + smoke_text(track.title) + ' ' + track.character;
        payload += ' ' + std::to_string(track.tags.size());
        for(const smoke_tag& tag : track.tags) {
            payload += ' ' + tag.wire_name + ' ' + tag.parameter;
        }
        payload += ' ' + std::to_string(track.mods.size());
        for(const std::string& mod : track.mods) {
            payload += ' ' + mod;
        }
    }
    smoke_feed(payload);
}

void smoke_push_order()
{
    const smoke_playlist* playlist = smoke_find_playlist(g_smoke_open_id);
    if(playlist == nullptr) {
        return;
    }

    // The upcoming queue is everything after the playing entry, capped the way the real host caps
    // it. Nothing draws it yet - it is carried so a Next/Prev request can be confirmed exactly.
    std::vector<std::string> upcoming;
    bool past_current = g_smoke_playing_id.empty();
    for(const smoke_track& track : playlist->tracks) {
        if(past_current && upcoming.size() < 25) {
            upcoming.push_back(track.id);
        }
        if(track.id == g_smoke_playing_id) {
            past_current = true;
        }
    }

    std::string payload = "QP_ORDER " + playlist->id + ' ' + playlist->mode + ' ' + playlist->advance + ' '
                          + (g_smoke_playing_id.empty() ? "-" : g_smoke_playing_id) + ' ' + std::to_string(upcoming.size());
    for(const std::string& id : upcoming) {
        payload += ' ' + id;
    }
    smoke_feed(payload);
}

void smoke_push_playback_state()
{
    if(g_smoke_playing_id.empty()) {
        smoke_feed("QP_STOPPED");
        return;
    }
    smoke_feed("QP_NOWPLAYING " + g_smoke_open_id + ' ' + g_smoke_playing_id);
}

void smoke_push_default_character()
{
    smoke_feed("QP_DEFAULT_CHARACTER " + g_smoke_default_character);
}

// Applies a QP_NOTIFY_* request to the fake model and echoes the result back, which is exactly the
// contract the real host has: the overlay never mutates its own Quick Player state, it asks and
// waits for the authoritative answer.
void smoke_handle_quick_player_request(std::string_view op, std::string_view rest)
{
    const auto next = [&rest]() -> std::string {
        const auto space = rest.find(' ');
        std::string token { space == std::string_view::npos ? rest : rest.substr(0, space) };
        rest = space == std::string_view::npos ? std::string_view {} : rest.substr(space + 1);
        return token;
    };

    if(op == "QP_NOTIFY_SELECT") {
        g_smoke_open_id = next();
        smoke_push_tracks(g_smoke_open_id);
        smoke_push_order();
        return;
    }

    if(op == "QP_NOTIFY_PLAY") {
        g_smoke_open_id = next();
        g_smoke_playing_id = next();
        smoke_push_playback_state();
        smoke_push_order();
        return;
    }

    if(op == "QP_NOTIFY_TRANSPORT") {
        const std::string action = next();
        const smoke_playlist* playlist = smoke_find_playlist(g_smoke_open_id);
        if(playlist == nullptr || playlist->tracks.empty()) {
            return;
        }

        int index = -1;
        for(std::size_t i = 0; i < playlist->tracks.size(); ++i) {
            if(playlist->tracks[i].id == g_smoke_playing_id) {
                index = static_cast<int>(i);
                break;
            }
        }

        if(action == "stop") {
            g_smoke_playing_id.clear();
        }
        else if(action == "next") {
            index = (index + 1) % static_cast<int>(playlist->tracks.size());
            g_smoke_playing_id = playlist->tracks[static_cast<std::size_t>(index)].id;
        }
        else if(action == "prev") {
            index = index <= 0 ? static_cast<int>(playlist->tracks.size()) - 1 : index - 1;
            g_smoke_playing_id = playlist->tracks[static_cast<std::size_t>(index)].id;
        }

        smoke_push_playback_state();
        smoke_push_order();
        return;
    }

    if(op == "QP_NOTIFY_MODE" || op == "QP_NOTIFY_ADVANCE") {
        smoke_playlist* playlist = smoke_find_playlist(next());
        if(playlist == nullptr) {
            return;
        }

        (op == "QP_NOTIFY_MODE" ? playlist->mode : playlist->advance) = next();
        smoke_push_order();
        return;
    }

    if(op == "QP_NOTIFY_DEFAULT_CHARACTER") {
        g_smoke_default_character = next();
        smoke_push_default_character();
        return;
    }

    // Per-track edits. The fake host applies them the way the real one does - by rewriting the
    // entry and echoing the whole playlist back - so the overlay's optimistic display is confirmed
    // by real state rather than by its own assumption.
    if(op == "QP_NOTIFY_TAG" || op == "QP_NOTIFY_MOD" || op == "QP_NOTIFY_CHARACTER") {
        const std::string entry_id = next();
        smoke_track* track = nullptr;
        for(smoke_playlist& playlist : g_smoke_playlists) {
            for(smoke_track& candidate : playlist.tracks) {
                if(candidate.id == entry_id) {
                    track = &candidate;
                }
            }
        }
        if(track == nullptr) {
            return;
        }

        if(op == "QP_NOTIFY_CHARACTER") {
            track->character = next();
        }
        else if(op == "QP_NOTIFY_MOD") {
            const std::string wire_name = next();
            const bool enabled = next() == "true";
            std::erase(track->mods, wire_name);
            if(enabled) {
                track->mods.push_back(wire_name);
            }
        }
        else {
            const std::string wire_name = next();
            const bool enabled = next() == "true";
            const std::string parameter = next();
            std::erase_if(track->tags, [&wire_name](const smoke_tag& tag) {
                return tag.wire_name == wire_name;
            });
            if(enabled) {
                track->tags.push_back({ wire_name, parameter });
            }
        }

        smoke_push_tracks(g_smoke_open_id);
        return;
    }
}

void seed_fake_quick_player()
{
    smoke_playlist mixtape { .id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa1", .name = "Mixtape" };
    mixtape.tracks.push_back({ .id = "1000000000000000000000000000000a",
        .artist = "Boards of Canada",
        .title = "Roygbiv",
        .character = "NinjaMono",
        .tags = { { "FourLanes" }, { "MinimumMatchSize", "12" } },
        .mods = { "InvisibleRoad" } });
    mixtape.tracks.push_back({ .id = "1000000000000000000000000000000b", .artist = "", .title = "Untitled [demo]" });
    mixtape.tracks.push_back({ .id = "1000000000000000000000000000000c",
        .artist = "Aphex Twin",
        .title = "Xtal",
        .tags = { { "Portal" } },
        .mods = { "BankingCamera", "HiddenSongTitle" } });

    // Deliberately long: this is the case ImGuiListClipper exists for, and the only way to notice
    // it regressing is to have a list here that would visibly stutter without it.
    smoke_playlist grind { .id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2", .name = "Grind", .mode = "RepeatOne", .advance = "manual" };
    for(int i = 0; i < 250; ++i) {
        char id[33];
        std::snprintf(id, sizeof(id), "2000000000000000000000000000%04d", i);
        grind.tracks.push_back({ .id = id, .artist = "Practice", .title = "Chart #" + std::to_string(i + 1) });
    }

    smoke_playlist empty { .id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa3", .name = "Freeride", .mode = "ShuffleLoop" };

    g_smoke_playlists = { std::move(mixtape), std::move(grind), std::move(empty) };
    g_smoke_open_id = g_smoke_playlists.front().id;
    g_smoke_playing_id = g_smoke_playlists.front().tracks.front().id;

    smoke_push_catalog();
    smoke_push_default_character();
    smoke_push_tracks(g_smoke_open_id);
    smoke_push_order();
    smoke_push_playback_state();
}

// pending_actions' send backend (see ui/pending_actions.hxx) - stands in for tw::ipc::send_overlay_command,
// which doesn't exist in this build (no real TW_OVL IPC here). When g_smoke_auto_confirm is on, applies
// the request straight into overlay_state, as if the host had echoed TWEAK_SET/CURRENT_SKIN back
// immediately; when off, it's a no-op "sent into the void" so pending_actions' own deadline fires.
bool smoke_send_overlay_command(std::string_view op_line)
{
    const auto space = op_line.find(' ');
    const std::string_view op = space == std::string_view::npos ? op_line : op_line.substr(0, space);
    const std::string_view rest = space == std::string_view::npos ? std::string_view {} : op_line.substr(space + 1);

    // Quick Player requests are always answered, regardless of the auto-confirm switch: that switch
    // exists to exercise pending_actions' timeout path for tweaks/skins, and a Player tab that
    // could not load a playlist would just be inert rather than instructive.
    if(op.starts_with("QP_NOTIFY_")) {
        smoke_handle_quick_player_request(op, rest);
        return true;
    }

    if(!g_smoke_auto_confirm) {
        return true;
    }

    if(op == "NOTIFY_TWEAK") {
        const auto sp2 = rest.find(' ');
        if(sp2 == std::string_view::npos) {
            return true;
        }
        const auto id = tw::ui::overlay_state::resolve_tweak_id(rest.substr(0, sp2));
        const bool enabled = rest.substr(sp2 + 1) == "true";
        tw::ui::overlay_state::set_tweak(id, enabled);
    }
    else if(op == "NOTIFY_SKIN") {
        tw::ui::overlay_state::set_current_skin(smoke_percent_decode(rest));
    }

    return true;
}

// Bypasses real TW_OVL IPC entirely - overlay_state's setters are public API, so smoke just calls
// them directly to get plausible data flowing into notefeed/pins/menu.
void seed_fake_overlay_state()
{
    using tw::ui::overlay_state::tweak_id;

    // First: going online is what the real plugin does on HANDSHAKE_BEGIN, before the host pushes anything,
    // and going offline would clear everything seeded below.
    tw::ui::overlay_state::set_host_connected(true);

    std::vector<std::string> skins = {
        "Neon Pulse",
        "Mono Track",
        "Chrome Wave",
        "Void Runner",
        "Pixel Drift",
        "Solar Flare",
        "Aurora Drift",
        "Carbon Night",
    };
    tw::ui::overlay_state::set_skin_list(std::move(skins));
    tw::ui::overlay_state::set_current_skin("Neon Pulse");
    tw::ui::overlay_state::set_tweak(tweak_id::invisible_road, true);
    tw::ui::overlay_state::set_tweak(tweak_id::sidewinder_camera, true, true);
}
} // namespace

namespace tw::smoke
{
void seed_fake_overlay_state()
{
    ::seed_fake_overlay_state();
}

void seed_fake_quick_player()
{
    ::seed_fake_quick_player();
}

bool send_overlay_command(std::string_view op_line)
{
    return smoke_send_overlay_command(op_line);
}

bool& auto_confirm() noexcept
{
    return g_smoke_auto_confirm;
}
} // namespace tw::smoke
