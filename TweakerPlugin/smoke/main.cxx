// Visual smoke harness for the real TweakerPlugin overlay UI (Win32 + OpenGL3) - notefeed, pins,
// watermark, and the interactive menu, driven by fake overlay_state data instead of real TW_OVL
// IPC. See Docs/Internal/tweaker-plugin-widgets.md.
// Build: cmake --build --preset x86-release --target smoke_test
//
// Smoke is a standalone exe (no TweakerPlugin pch.hxx / Quest3D) — library
// includes live here. Plugin sources use pch.hxx instead.

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <GL/gl.h>
#include <tchar.h>

#include <cstdio>
#include <expected>
#include <span>
#include <string>
#include <vector>

// resource.hxx uses std::span/std::expected without including them itself (relies on the
// TweakerPlugin PCH in DLL consumers) - explicit here, same convention as ui/texture_cache.cxx.
#include "resource/resource.hxx"

#include "ui/gpu_texture.hxx"
#include "ui/image/svg.hxx"
#include "ui/overlay_config.hxx"
#include "ui/overlay_state.hxx"
#include "ui/pending_actions.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/plugins/static/pins.hxx"
#include "ui/plugins/static/watermark.hxx"
#include "ui/qp/qp_pending.hxx"
#include "ui/qp/qp_state.hxx"
#include "ui/qp/qp_wire.hxx"
#include "ui/texture_cache.hxx"
#include "ui/theme.hxx"
#include "ui/wire_text.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/color_picker.hxx"
#include "ui/widgets/item_group.hxx"
#include "ui/widgets/list_item.hxx"
#include "ui/widgets/number_input.hxx"
#include "ui/widgets/popup_menu.hxx"
#include "ui/widgets/segmented.hxx"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
struct wgl_window_data {
    HDC hdc = nullptr;
};

HGLRC g_hRC = nullptr;
wgl_window_data g_main_window {};
int g_width = 0;
int g_height = 0;

bool create_device_wgl(HWND hwnd, wgl_window_data* data);
void cleanup_device_wgl(HWND hwnd, wgl_window_data* data);
LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// gpu_texture's pluggable backend (see ui/gpu_texture.hxx) - the DLL registers the D3D9 pair
// (framework/imgui_backend.cxx), this registers the GL equivalent so the exact same
// notefeed/pins/watermark/menu code can load real packed icons here too.
ImTextureID gl_upload_texture(const unsigned char* rgba, int width, int height)
{
    if(rgba == nullptr || width <= 0 || height <= 0) {
        return ImTextureID_Invalid;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Explicit clamp (not the GL_REPEAT default) - default wrap mode on non-power-of-two textures
    // is where legacy/compatibility GL contexts are most likely to sample garbage at the edges.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex != 0 ? static_cast<ImTextureID>(tex) : ImTextureID_Invalid;
}

void gl_release_texture(ImTextureID tex)
{
    const auto name = static_cast<GLuint>(tex);
    if(name != 0) {
        glDeleteTextures(1, &name);
    }
}

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

void draw_smoke_controls()
{
    using namespace tw::ui::widgets;

    static button push_toast_btn { "smoke_push_toast", { 200.f, 32.f } };
    static button toggle_skin_btn { "smoke_toggle_skin", { 200.f, 32.f } };
    static button toggle_qp_btn { "smoke_toggle_qp", { 200.f, 32.f } };
    static button open_popup_btn { "smoke_open_popup", { 200.f, 32.f } };
    static item_group actions { "smoke_actions", "Actions" };
    static item_group controls_group { "smoke_controls", "Controls" };
    static item_group icons_group { "smoke_icons", "Icons" };
    static segmented modes { "smoke_modes" };
    static segmented advance { "smoke_advance" };
    static number_input match_size { "smoke_match_size" };
    static button play_btn { "smoke_play", { 40.f, 34.f } };
    static button labelled_btn { "smoke_labelled", { 150.f, 34.f } };
    static bool modes_ready = false;
    static item_group color_group { "smoke_color", "Color" };
    static color_picker accent_picker { "smoke_accent", { 280.f, 0.f } };
    static ImVec4 bound_color { 0.2f, 0.83f, 0.75f, 1.f };
    static popup_menu ctx_menu { "smoke_ctx_menu", "Context" };
    static list_item ctx_copy { "smoke_ctx_copy", { 160.f, 28.f } };
    static list_item ctx_paste { "smoke_ctx_paste", { 160.f, 28.f } };
    static int toast_count = 0;
    static bool alt_skin = false;
    static bool qp_override = true;
    static int ctx_action = 0;

    ImGui::SetNextWindowSize(ImVec2 { 360.f, 520.f }, ImGuiCond_FirstUseEver);
    ImGui::Begin("Smoke controls");

    ImGui::TextUnformatted("Real TweakerPlugin overlay modules, fed with fake overlay_state data.");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::TextUnformatted("Press Insert to toggle the menu (drag title bar to move, corner to resize).");
    ImGui::TextUnformatted("RMB in this window opens popup_menu; LMB on Open popup too.");
    ImGui::Separator();

    actions.set_inner_padding({ 10.f, 8.f });
    actions.begin();
    push_toast_btn.update("Push notefeed toast");
    if(push_toast_btn.clicked()) {
        ++toast_count;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Test notification #%d", toast_count);
        tw::ui::plugins::statics::notefeed::push(buf, tw::ui::overlay_state::skin_icon_key());
    }

    toggle_skin_btn.update("Toggle current skin");
    if(toggle_skin_btn.clicked()) {
        alt_skin = !alt_skin;
        tw::ui::overlay_state::set_current_skin(alt_skin ? "Void Runner" : "Neon Pulse");
    }

    toggle_qp_btn.update(qp_override ? "Clear QP marker" : "Set QP marker");
    if(toggle_qp_btn.clicked()) {
        qp_override = !qp_override;
        tw::ui::overlay_state::set_tweak(tw::ui::overlay_state::tweak_id::sidewinder_camera, true, qp_override);
    }

    open_popup_btn.update("Open popup (LMB)");
    if(open_popup_btn.clicked()) {
        ctx_menu.open();
    }
    ImGui::Checkbox("Auto-confirm NOTIFY_TWEAK/NOTIFY_SKIN (simulate host online)", &g_smoke_auto_confirm);
    ImGui::TextUnformatted("Off = requests time out after ~5s and show a notefeed failure toast.");
    actions.end();

    // The bench for the widgets Quick Player's tab is built from: the mode strip, a parameterized
    // tag's numeric field, and an icon-only transport button.
    controls_group.set_inner_padding({ 10.f, 8.f });
    controls_group.begin();

    if(!modes_ready) {
        static const std::string_view k_mode_labels[] = { "Single", "In order", "Repeat one", "Repeat all", "Shuffle", "Shuffle loop" };
        modes.set_options(k_mode_labels);
        static const std::string_view k_advance_labels[] = { "Auto", "Manual" };
        advance.set_options(k_advance_labels);
        match_size.set_range(0, 24); // SongTagCatalog's real bounds for [as-msz<n>]
        match_size.set_value(8);
        modes_ready = true;
    }

    ImGui::TextUnformatted("Playback");
    modes.update();
    if(modes.hovered_index() >= 0) {
        ImGui::SetTooltip("Mode %d - tooltips are the caller's job, the strip only reports the hover.", modes.hovered_index());
    }

    ImGui::TextUnformatted("Advance");
    advance.update();

    ImGui::TextUnformatted("Minimum match size");
    match_size.update();
    if(match_size.changed()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "[as-msz%d]", match_size.value());
        tw::ui::plugins::statics::notefeed::push(buf);
    }

    play_btn.set_icon("icons/play.svg");
    play_btn.update();
    ImGui::SameLine();
    labelled_btn.set_icon("icons/music.svg");
    labelled_btn.update("With label");

    controls_group.end();

    // The bench for ui/image/svg: every packed icon, at each baked size plus one that has to be
    // rasterized on demand through image::at(). This is where the k_supersample_factor question is
    // settled - flip it in src/ui/image/svg.cxx, rebuild, and compare the 16px column.
    icons_group.set_inner_padding({ 10.f, 8.f });
    icons_group.begin();
    const auto icon_keys = tw::resource::list_keys(tw::resource::type::vector);
    if(icon_keys.empty()) {
        ImGui::TextUnformatted("No TW_SVG resources packed (see assets/icons/).");
    }
    for(const std::string_view key : icon_keys) {
        const auto icon = tw::ui::image::svg::get_resource(key);
        // Tinted with the theme's text colour, the way the real modules do it - these assets carry
        // shape in the alpha channel only.
        const ImVec4 tint = tw::ui::theme::text_primary;
        for(const int px : { 16, 24, 32, 48 }) {
            const ImVec2 size { static_cast<float>(px), static_cast<float>(px) };
            const ImTextureID tex = icon.at(px);
            if(tex == ImTextureID_Invalid) {
                // Never draw a null texture: with no texture bound, the backend fills the quad with
                // the flat tint colour, which reads as a solid block rather than "nothing here".
                ImGui::Dummy(size);
            }
            else {
                ImGui::ImageWithBg(tex, size, ImVec2 { 0.f, 0.f }, ImVec2 { 1.f, 1.f }, ImVec4 { 0.f, 0.f, 0.f, 0.f }, tint);
            }
            ImGui::SameLine();
        }
        // Trailing SameLine above would otherwise glue the label of the next row onto this one.
        ImGui::TextUnformatted(key.data(), key.data() + key.size());
    }
    icons_group.end();

    color_group.set_inner_padding({ 10.f, 8.f });
    color_group.begin();
    accent_picker.update(bound_color);
    ImGui::ColorButton("##smoke_swatch", bound_color, ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2 { 48.f, 24.f });
    ImGui::SameLine();
    ImGui::Text(
        "RGBA %.2f %.2f %.2f %.2f%s",
        bound_color.x,
        bound_color.y,
        bound_color.z,
        bound_color.w,
        accent_picker.changed() ? " *" : "");
    color_group.end();

    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
        && !ctx_menu.opened()) {
        ctx_menu.open();
    }

    if(ctx_action != 0) {
        ImGui::Text("Last popup action: %s", ctx_action == 1 ? "Copy" : "Paste");
    }

    ImGui::End();

    if(ctx_menu.opened()) {
        ctx_menu.begin();
        ctx_copy.set_content({ .text = "Copy" });
        ctx_copy.update();
        if(ctx_copy.clicked()) {
            ctx_action = 1;
            ctx_menu.close();
        }
        ctx_paste.set_content({ .text = "Paste" });
        ctx_paste.update();
        if(ctx_paste.clicked()) {
            ctx_action = 2;
            ctx_menu.close();
        }
        ctx_menu.end();
    }
}

bool create_device_wgl(HWND hwnd, wgl_window_data* data)
{
    HDC hdc = ::GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hdc, &pfd);
    if(pf == 0) {
        ::ReleaseDC(hwnd, hdc);
        return false;
    }
    if(::SetPixelFormat(hdc, pf, &pfd) == FALSE) {
        ::ReleaseDC(hwnd, hdc);
        return false;
    }
    ::ReleaseDC(hwnd, hdc);

    data->hdc = ::GetDC(hwnd);
    if(!g_hRC) {
        g_hRC = wglCreateContext(data->hdc);
    }
    // != FALSE, not == TRUE: BOOL is an int, and "nonzero" is all a Win32 BOOL ever promises.
    return wglMakeCurrent(data->hdc, g_hRC) != FALSE;
}

void cleanup_device_wgl(HWND hwnd, wgl_window_data* data)
{
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hwnd, data->hdc);
}

LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    switch(msg) {
        // Mirrors the DLL's own toggle-key subscriber (ui_main.cxx, handle_toggle_key): the menu
        // no longer polls the keyboard, the harness that owns the message pump tells it when the
        // hotkey was pressed. Bit 30 of lparam is the previous key state - set means auto-repeat.
        case WM_KEYDOWN:
            if(wparam == VK_INSERT && (lparam & (LPARAM { 1 } << 30)) == 0) {
                tw::ui::plugins::interactive::menu::toggle_visible();
            }
            return 0;
        case WM_SIZE:
            if(wparam != SIZE_MINIMIZED) {
                g_width = LOWORD(lparam);
                g_height = HIWORD(lparam);
            }
            return 0;
        case WM_SYSCOMMAND:
            if((wparam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}
} // namespace

int main(int, char**)
{
    if(!tw::resource::initialize(::GetModuleHandleW(nullptr))) {
        std::fprintf(stderr, "smoke_test: resource::initialize failed\n");
        return 1;
    }

    const auto font = tw::resource::get_resource(tw::resource::type::font, "fonts/Roboto-Regular.ttf");
    if(!font || font->bytes.empty()) {
        std::fprintf(stderr, "smoke_test: missing embedded font fonts/Roboto-Regular.ttf\n");
        return 1;
    }

    tw::ui::gpu_texture::set_backend(&gl_upload_texture, &gl_release_texture);
    // Indexes the packed SVGs. The bake itself needs a GL context, so it waits for svg::update() in
    // the frame loop below - same two-step the DLL goes through.
    tw::ui::image::svg::initialize();
    tw::ui::pending_actions::set_send_backend(&smoke_send_overlay_command);
    tw::ui::qp::set_send_backend(&smoke_send_overlay_command);
    tw::ui::overlay_config::load("smoke_overlay.cfg");
    seed_fake_overlay_state();
    seed_fake_quick_player();

    // Land straight on the Player tab: it is the one with the most moving parts, and starting every
    // run with Insert plus a click adds nothing.
    tw::ui::plugins::interactive::menu::show_tab(2);

    tw::ui::theme::apply_dark();
    tw::ui::theme::from_config(tw::ui::overlay_config::theme_overrides());

    ImGui_ImplWin32_EnableDpiAwareness();
    const float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT { 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_OWNDC,
        wnd_proc,
        0L,
        0L,
        GetModuleHandle(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"TweakerUiSmoke",
        nullptr,
    };
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowW(wc.lpszClassName,
        L"TweakerPlugin UI Smoke",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        static_cast<int>(900 * main_scale),
        static_cast<int>(700 * main_scale),
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);

    if(!create_device_wgl(hwnd, &g_main_window)) {
        cleanup_device_wgl(hwnd, &g_main_window);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        std::fprintf(stderr, "smoke_test: failed to create OpenGL context\n");
        return 1;
    }
    wglMakeCurrent(g_main_window.hdc, g_hRC);

    // Cap to the display's refresh rate. Uncapped, this window renders at thousands of FPS, which is
    // not what the overlay ever sees inside the game - and judging animation timing here (the whole
    // point of the harness) against a frame rate the real thing never reaches is misleading. Best
    // effort: the extension is absent on some drivers, and its absence is not worth failing over.
    if(const auto swap_interval = reinterpret_cast<BOOL(WINAPI*)(int)>(wglGetProcAddress("wglSwapIntervalEXT"))) {
        swap_interval(1);
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    {
        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false; // bytes live in PE LockResource memory
        ImFont* loaded = io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(static_cast<const void*>(font->bytes.data())),
            static_cast<int>(font->bytes.size()),
            18.0f * main_scale,
            &font_cfg);
        if(loaded == nullptr) {
            std::fprintf(stderr, "smoke_test: AddFontFromMemoryTTF failed\n");
            ImGui::DestroyContext();
            cleanup_device_wgl(hwnd, &g_main_window);
            wglDeleteContext(g_hRC);
            ::DestroyWindow(hwnd);
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return 1;
        }
        std::fprintf(stderr, "smoke_test: Roboto-Regular registered with ImGui\n");
    }

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init();

    tw::ui::plugins::statics::watermark::initialize();
    tw::ui::plugins::statics::pins::initialize();
    tw::ui::plugins::statics::notefeed::initialize();
    tw::ui::plugins::interactive::menu::initialize();

    // Smoke-harness-only backdrop - a real overlay has no window background to paint, so this
    // isn't part of tw::ui::theme (matches former theme::app_background's dark value, #FF1B1D21).
    constexpr ImVec4 clear_color { 0x1B / 255.f, 0x1D / 255.f, 0x21 / 255.f, 1.f };

    bool done = false;
    while(!done) {
        MSG msg;
        while(::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if(msg.message == WM_QUIT) {
                done = true;
            }
        }
        if(done) {
            break;
        }
        if(::IsIconic(hwnd)) {
            ::Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Bakes every packed SVG on the first frame, exactly as ui_main::draw_frame does.
        tw::ui::image::svg::update();

        static tw::ui::overlay_state::cache cache;
        tw::ui::overlay_state::refresh(cache);
        tw::ui::pending_actions::update(cache);

        static tw::ui::qp::state::cache qp_cache;
        tw::ui::qp::state::refresh(qp_cache);
        tw::ui::qp::pending::update(qp_cache);

        draw_smoke_controls();
        tw::ui::plugins::statics::watermark::update();
        tw::ui::plugins::statics::pins::update(cache);
        tw::ui::plugins::statics::notefeed::update();
        tw::ui::plugins::interactive::menu::update(cache, qp_cache);

        ImGui::Render();
        glViewport(0, 0, g_width, g_height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ::SwapBuffers(g_main_window.hdc);
    }

    tw::ui::overlay_config::save();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    cleanup_device_wgl(hwnd, &g_main_window);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
