// No TweakerPlugin PCH here - shared with smoke_test, same convention as menu.cxx.
#include "ui/plugins/interactive/player.hxx"

#include "ui/overlay_state.hxx"
#include "ui/qp/qp_catalog.hxx"
#include "ui/qp/qp_pending.hxx"
#include "ui/qp/qp_wire.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/detail/draw.hxx"
#include "ui/widgets/list_item.hxx"
#include "ui/widgets/list_view.hxx"
#include "ui/widgets/number_input.hxx"
#include "ui/widgets/popup_menu.hxx"
#include "ui/widgets/segmented.hxx"
#include "ui/widgets/tab_view.hxx"
#include "ui/widgets/toggle.hxx"

#include <imgui.h>
#include <imgui_internal.h>

#include <libtweeny/tweeny.hxx>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace qp = tw::ui::qp;
namespace state = tw::ui::qp::state;
namespace theme = tw::ui::theme;
namespace detail = tw::ui::widgets::detail;

using tw::ui::widgets::button;
using tw::ui::widgets::list_item;
using tw::ui::widgets::list_item_content;
using tw::ui::widgets::list_view;
using tw::ui::widgets::number_input;
using tw::ui::widgets::popup_menu;
using tw::ui::widgets::segmented;
using tw::ui::widgets::tab_view;
using tw::ui::widgets::toggle;

constexpr float k_playlist_col_w = 190.f;
constexpr float k_col_gap = 8.f;
constexpr float k_row_pad_x = 10.f;
constexpr float k_row_pad_y = 6.f;
constexpr float k_line_gap = 4.f;
constexpr float k_marker_w = 3.f;

// A track row grows to fit its badges instead of dropping the ones that do not fit, which is what
// the desktop card does (its badges live in a WrapPanel) and what the first pass did not
// (Fuckups/tags_did_not_fits.png). Badges therefore flow from the title's line downwards, and the
// artist is always the last line.
constexpr float k_badge_line_gap = 3.f;
constexpr float k_min_badge_area = 140.f; // reserved from the title so badges always get a first run

// Badges get real breathing room on both axes. The first pass gave them 6px horizontally and 2px
// vertically, which read as text with a tint behind it rather than as the desktop's pills
// (Fuckups/tags_too_small.png), and let the last one sit flush against the row's edge.
constexpr float k_badge_gap = 6.f;
constexpr float k_badge_pad_x = 8.f;
constexpr float k_badge_pad_y = 3.f;
constexpr float k_badge_margin = 12.f; // kept clear at the row's right edge

constexpr float k_transport_btn = 36.f;
constexpr float k_strip_h = 28.f;
constexpr float k_char_row_h = 28.f;
constexpr float k_divider_w = 1.f;
constexpr float k_divider_margin = 10.f;
constexpr float k_group_gap = 8.f; // between a caption and the strip it labels
constexpr float k_min_track_col_w = 340.f;
constexpr float k_min_body_h = 120.f;

// Same durations and easings the rest of the widget set uses (see the animation table in
// Docs/Internal/tweaker-plugin-widgets.md) - the list has no business feeling different.
constexpr int k_hover_ms = 150;
constexpr int k_select_ms = 150;

// One prepared track row. Badge strings are built when the track list changes rather than every
// frame: the list is drawn from EndScene, and formatting a dozen strings per visible row per frame
// is exactly the kind of allocation churn the hot-path rules exist to keep out. The backing vector
// is pinned by g_rows_source so these pointers stay valid for as long as the rows do.
struct badge_placement {
    float x = 0.f;
    float width = 0.f;
    int line = 0;
};

struct row_view {
    const state::track* track = nullptr;
    std::vector<std::string> badges;

    // Filled by layout_rows(), which is the single place the flow is decided. Drawing reads these
    // rather than re-deriving them: two copies of the wrapping arithmetic would eventually disagree,
    // and the row's height is computed from this one.
    std::vector<badge_placement> badge_layout;
    float height = 0.f;
    float title_max_x = 0.f; // relative to the row's left edge
    int badge_lines = 0;
};

list_view g_playlists { "qp_playlists" };
std::vector<std::string> g_playlist_ids;
const void* g_seen_catalog = nullptr;

std::shared_ptr<const state::track_list> g_rows_source;
std::vector<row_view> g_rows;

// Running sum of row heights, one entry longer than g_rows - rows have different heights now, so
// culling cannot be "scroll / row height" and ImGuiListClipper's fixed-height mode no longer
// applies. A prefix sum turns both ends of the visible range into a binary search.
std::vector<float> g_row_offsets;
float g_rows_layout_width = -1.f;

int g_selected_track = -1;

// Row highlight animation. Per-row tween state is what a virtualized list cannot have - a float
// attached to row index 12 animates whatever scrolled into that slot, not the row the mouse is on.
// But at any instant there is exactly one hovered row and one selected row, so the animation belongs
// to *those*, with the row they moved away from kept for the length of one crossfade. Same
// previous/current pair tab_view uses for its own content transition.
int g_hover_row = -1;
int g_prev_hover_row = -1;
float g_hover_t = 1.f;
tweeny::tween<float> g_hover_tween;

int g_prev_selected_row = -1;
float g_select_t = 1.f;
tweeny::tween<float> g_select_tween;

// Set by any row that reports hover this frame. A row under the mouse is always inside the clipper's
// range, so "no row set this" means the pointer left the list - which has to fade the highlight out,
// not leave it stuck on the last row touched.
bool g_hover_seen_this_frame = false;

// What the overlay asked to open, so a click highlights immediately instead of waiting for the
// round trip that brings the tracks back.
std::string g_requested_playlist_id;

segmented g_modes { "qp_modes", k_strip_h };
segmented g_advance { "qp_advance", k_strip_h };
button g_prev_btn { "qp_prev", { k_transport_btn, k_transport_btn } };
button g_play_btn { "qp_play", { k_transport_btn, k_transport_btn } };
button g_stop_btn { "qp_stop", { k_transport_btn, k_transport_btn } };
button g_next_btn { "qp_next", { k_transport_btn, k_transport_btn } };
button g_character_btn { "qp_character", { 150.f, 26.f } };
popup_menu g_character_popup { "qp_character_popup", "Default character" };
std::vector<list_item> g_character_rows;

// Right-click context menu for one track: character override, tags, mods - the overlay's answer to
// the desktop card's own right-click editor. Three tabs rather than one long column because the
// full set is 14 characters plus 17 tags plus 7 mods, which no popup can show at once.
// Wide enough for the tag descriptions, which run to "Mono all greys - scores start at 1000 and go
// down with each hit". At 330px they were clipped mid-word (Fuckups/tags_too_small.png); the ones
// that still do not fit now wrap instead of being cut.
constexpr ImVec2 k_context_size { 460.f, 300.f };
constexpr float k_context_param_w = 110.f;

std::string g_context_entry_id;
popup_menu g_context_popup { "qp_track_ctx" };
tab_view g_context_tabs { "qp_ctx_tabs", k_context_size };
list_view g_context_characters { "qp_ctx_characters" };
std::vector<qp::tag_id> g_selectable_tags;
std::vector<toggle> g_tag_toggles;
std::vector<number_input> g_tag_params;
std::vector<toggle> g_mod_toggles;

bool g_widgets_ready = false;

void ensure_widgets_ready()
{
    if(g_widgets_ready) {
        return;
    }

    // Glyph chips, like the desktop's playback row: the six icons are ports of its own IconQPMode*
    // geometries, so the two surfaces read as one control. The names live in the tooltip instead.
    std::vector<std::string_view> mode_labels;
    std::vector<std::string_view> mode_icons;
    mode_labels.reserve(qp::all_playback_modes().size());
    mode_icons.reserve(qp::all_playback_modes().size());
    for(const auto& mode : qp::all_playback_modes()) {
        mode_labels.push_back(mode.display_name);
        mode_icons.push_back(mode.icon_key);
    }
    g_modes.set_options(mode_labels);
    g_modes.set_icons(mode_icons);

    static const std::string_view k_advance_labels[] = { "Auto", "Manual" };
    g_advance.set_options(k_advance_labels);

    g_prev_btn.set_icon("icons/prev.svg");
    g_play_btn.set_icon("icons/play.svg");
    g_stop_btn.set_icon("icons/stop.svg");
    g_next_btn.set_icon("icons/next.svg");

    g_character_rows.clear();
    g_character_rows.reserve(qp::all_characters().size());
    for(std::size_t i = 0; i < qp::all_characters().size(); ++i) {
        const std::string id = "qp_character_" + std::to_string(i);
        g_character_rows.emplace_back(id.c_str(), ImVec2 { 168.f, 26.f });
    }

    static const std::string_view k_context_labels[] = { "Character", "Tags", "Mods" };
    g_context_tabs.set_tabs(k_context_labels);

    // Only the tags the host will actually accept in PlaylistEntry.Tags. Sidewinder and Banking
    // Camera are filtered out here exactly as TagOptionViewModel filters them out of the desktop's
    // Tags list - they live among the mods on both surfaces.
    g_selectable_tags.clear();
    g_tag_toggles.clear();
    g_tag_params.clear();
    for(const auto& definition : qp::all_tags()) {
        if(!definition.selectable) {
            continue;
        }

        const std::size_t index = g_selectable_tags.size();
        g_selectable_tags.push_back(definition.id);

        const std::string toggle_id = "qp_tag_" + std::to_string(index);
        g_tag_toggles.emplace_back(toggle_id.c_str());

        const std::string param_id = "qp_tag_param_" + std::to_string(index);
        g_tag_params.emplace_back(param_id.c_str(), 96.f);
        g_tag_params.back().set_range(definition.min_parameter, definition.max_parameter);
    }

    g_mod_toggles.clear();
    g_mod_toggles.reserve(tw::ui::overlay_state::k_tweak_count);
    for(std::size_t i = 0; i < tw::ui::overlay_state::k_tweak_count; ++i) {
        const std::string id = "qp_mod_" + std::to_string(i);
        g_mod_toggles.emplace_back(id.c_str());
    }

    // "No override" first, then the roster - the extra row is why this cannot just index
    // all_characters() directly.
    std::vector<list_item_content> character_items;
    character_items.emplace_back("No override");
    for(const auto& character : qp::all_characters()) {
        character_items.emplace_back(std::string(character.display_name));
    }
    g_context_characters.set_items(character_items);

    g_widgets_ready = true;
}

// Every label the desktop card shows on a track, in the same order: the character override first,
// then tags, then mods.
std::vector<std::string> build_badges(const state::track& entry)
{
    std::vector<std::string> badges;

    if(entry.character != qp::character_id::none) {
        badges.emplace_back(qp::character_display_name(entry.character));
    }

    for(const state::tag_state& tag : entry.tags) {
        badges.emplace_back(qp::format_tag(tag.id, tag.parameter, tag.has_parameter));
    }

    for(const tw::ui::overlay_state::tweak_id mod : entry.mods) {
        badges.emplace_back(tw::ui::overlay_state::tweak_display_name(mod));
    }

    return badges;
}

// Catalog and track list are both published as fresh shared_ptr-to-const on every change (see
// qp_state), so pointer identity is an exact and O(1) "has this changed" test - no deep comparison,
// and no rebuilding the rows on frames where only the play position moved.
void sync_playlists(const state::cache& snapshot)
{
    const state::playlist_list& playlists = state::playlists_of(snapshot);
    if(snapshot.playlists.get() == g_seen_catalog) {
        return;
    }
    g_seen_catalog = snapshot.playlists.get();

    std::vector<list_item_content> items;
    items.reserve(playlists.size());
    g_playlist_ids.clear();
    g_playlist_ids.reserve(playlists.size());

    for(const state::playlist_summary& playlist : playlists) {
        // Count on its own smaller line rather than "(6)" glued to the name: it competed with the
        // name for the one line they shared, and that line is already short enough to be truncating
        // real playlist names (Fuckups/playlist_name_too_long.png). Same split the desktop sidebar
        // uses (QPPlaylistName over QPPlaylistCount).
        items.emplace_back(playlist.name,
            std::to_string(playlist.track_count) + (playlist.track_count == 1 ? " track" : " tracks"));
        g_playlist_ids.emplace_back(playlist.playlist_id);
    }

    g_playlists.set_items(items);
}

void sync_rows(const state::cache& snapshot)
{
    if(snapshot.tracks.get() == g_rows_source.get()) {
        return;
    }

    g_rows_source = snapshot.tracks;
    g_rows.clear();

    const state::track_list& tracks = state::tracks_of(snapshot);
    g_rows.reserve(tracks.size());
    for(const state::track& entry : tracks) {
        g_rows.emplace_back(&entry, build_badges(entry));
    }

    // Heights depend on the badges that were just rebuilt, so the cached layout is stale by
    // definition - force it to be recomputed on the next draw, where a width is known.
    g_rows_layout_width = -1.f;

    if(g_selected_track >= static_cast<int>(g_rows.size())) {
        g_selected_track = -1;
    }
}

// Exact height of everything under the two lists: the character summary row, the transport row, and
// the spacing ImGui itself inserts between them and above them.
//
// Exact matters here. The first version guessed, guessed low, and the few pixels of overflow gave
// the tab a permanent scrollbar with content parked behind it - which is worse than being a few
// pixels short, because the scrollbar then eats width from every row as well.
float footer_height()
{
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    return k_char_row_h + spacing + k_transport_btn + spacing;
}

// Flows every row's badges and derives its height. Runs when the track list changes or the column is
// resized, not per frame - it measures every badge of every track, which on a thousand-entry
// playlist is far too much to redo from EndScene each frame.
//
// Layout per row: the first badge run shares the title's line, using whatever the title leaves;
// further runs start at the text margin and get the full width, so a long title cannot squeeze the
// badges into a one-per-line column. The artist is always the last line.
void layout_rows(float content_w)
{
    g_rows_layout_width = content_w;
    g_row_offsets.assign(g_rows.size() + 1, 0.f);

    const float line_h = ImGui::GetFontSize();
    const float badge_h = line_h + k_badge_pad_y * 2.f;
    const float text_x = k_row_pad_x + k_marker_w;
    const float right_x = content_w - k_badge_margin;

    for(std::size_t i = 0; i < g_rows.size(); ++i) {
        row_view& row = g_rows[i];
        row.badge_layout.clear();
        row.badge_lines = 0;

        const bool has_badges = !row.badges.empty();
        const float title_budget =
            has_badges ? (std::max)(80.f, right_x - text_x - k_min_badge_area) : (right_x - text_x);
        row.title_max_x = text_x + title_budget;

        if(has_badges) {
            const float title_w = (std::min)(ImGui::CalcTextSize(row.track->title.c_str()).x, title_budget);

            int line = 0;
            float x = text_x + title_w + k_badge_gap * 2.f;
            for(const std::string& badge : row.badges) {
                const float w = ImGui::CalcTextSize(badge.c_str()).x + k_badge_pad_x * 2.f;

                // Wrap, except when the run is already at the margin - a badge wider than the whole
                // column would otherwise loop forever looking for a line it fits on.
                if(x + w > right_x && x > text_x) {
                    ++line;
                    x = text_x;
                }

                row.badge_layout.emplace_back(x, w, line);
                x += w + k_badge_gap;
            }

            row.badge_lines = line + 1;
        }

        const float first_line_h = has_badges ? (std::max)(line_h, badge_h) : line_h;
        const float extra_lines = static_cast<float>((std::max)(0, row.badge_lines - 1));

        row.height = k_row_pad_y * 2.f + first_line_h + extra_lines * (badge_h + k_badge_line_gap) + k_line_gap + line_h;
        g_row_offsets[i + 1] = g_row_offsets[i] + row.height;
    }
}

// Which playlist the tab is acting on. The one the host actually delivered tracks for, falling back
// to whatever was requested so mode/advance clicks still address the right playlist during the
// round trip that follows a selection.
std::string_view acting_playlist_id(const state::cache& snapshot)
{
    if(!snapshot.tracks_playlist_id.empty()) {
        return snapshot.tracks_playlist_id;
    }

    return g_requested_playlist_id;
}

void request_playlist(std::string_view playlist_id)
{
    if(playlist_id.empty()) {
        return;
    }

    g_requested_playlist_id = playlist_id;
    g_selected_track = -1;
    qp::send_select(playlist_id);
}

// Draws the badges at the positions layout_rows() already decided. `first_line_center` is the
// vertical middle of the title's line, so a pill taller than the text beside it stays centred on it.
void draw_badges(ImDrawList* draw, const row_view& row, float origin_x, float first_line_center)
{
    const float badge_h = ImGui::GetFontSize() + k_badge_pad_y * 2.f;

    for(std::size_t i = 0; i < row.badge_layout.size() && i < row.badges.size(); ++i) {
        const badge_placement& placement = row.badge_layout[i];
        const float center_y = first_line_center + static_cast<float>(placement.line) * (badge_h + k_badge_line_gap);
        const float top = center_y - badge_h * 0.5f;

        const ImVec2 b_min { origin_x + placement.x, top };
        const ImVec2 b_max { b_min.x + placement.width, top + badge_h };
        draw->AddRectFilled(b_min, b_max, detail::to_u32(theme::surface_badge), 5.f);

        const ImVec2 text_size = ImGui::CalcTextSize(row.badges[i].c_str());
        draw->AddText(ImVec2 { b_min.x + k_badge_pad_x, center_y - text_size.y * 0.5f },
            detail::to_u32(theme::text_muted),
            row.badges[i].c_str());
    }
}

// Returns true if the row was double-clicked, i.e. the user asked to play it.
bool draw_track_row(int index, const row_view& row, bool playing)
{
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const ImRect bb { pos, ImVec2 { pos.x + width, pos.y + row.height } };

    ImGui::ItemSize(bb);
    ImGui::PushID(index);
    const ImGuiID id = ImGui::GetID("##row");
    if(!ImGui::ItemAdd(bb, id)) {
        ImGui::PopID();
        return false;
    }

    bool hovered = false;
    bool held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    const bool double_clicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    ImGui::PopID();

    if(hovered) {
        g_hover_seen_this_frame = true;
    }

    if(pressed && g_selected_track != index) {
        g_prev_selected_row = g_selected_track;
        g_selected_track = index;
        g_select_t = 0.f;
        g_select_tween = tweeny::from(0.f).to(1.f).during(k_select_ms).via(tweeny::easing::cubicOut);
    }

    if(hovered && g_hover_row != index) {
        g_prev_hover_row = g_hover_row;
        g_hover_row = index;
        g_hover_t = 0.f;
        g_hover_tween = tweeny::from(0.f).to(1.f).during(k_hover_ms).via(tweeny::easing::cubicOut);
    }

    // The track's tags/mods/character editor, matching the desktop card's own right-click menu. Only
    // the id is remembered - the popup re-resolves the track every frame, because the row vector is
    // rebuilt by the very edits this menu makes.
    if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        g_selected_track = index;
        g_context_entry_id = row.track->entry_id;
        g_context_popup.open();
    }

    // Both highlights crossfade: the row being left keeps its fill at 1-t while the row being
    // entered comes up at t, so nothing pops.
    const float select_a = g_selected_track == index ? g_select_t : (g_prev_selected_row == index ? 1.f - g_select_t : 0.f);
    const float hover_a = g_hover_row == index ? g_hover_t : (g_prev_hover_row == index ? 1.f - g_hover_t : 0.f);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if(hover_a > 0.01f) {
        // Under the selection, not instead of it: hovering the selected row should deepen it rather
        // than swap one fill for another.
        const ImVec4 fill { theme::surface_row_hover.x, theme::surface_row_hover.y, theme::surface_row_hover.z,
            theme::surface_row_hover.w * hover_a };
        draw->AddRectFilled(bb.Min, bb.Max, detail::to_u32(fill), 6.f);
    }
    if(select_a > 0.01f) {
        const ImVec4 fill { theme::accent_soft.x, theme::accent_soft.y, theme::accent_soft.z, theme::accent_soft.w * select_a };
        draw->AddRectFilled(bb.Min, bb.Max, detail::to_u32(fill), 6.f);
    }

    if(playing) {
        draw->AddRectFilled(bb.Min, ImVec2 { bb.Min.x + k_marker_w, bb.Max.y }, detail::to_u32(theme::accent_primary), 1.5f);
    }

    const float text_x = bb.Min.x + k_row_pad_x + k_marker_w;
    const float line_h = ImGui::GetFontSize();
    const float badge_h = line_h + k_badge_pad_y * 2.f;
    const float first_line_h = row.badges.empty() ? line_h : (std::max)(line_h, badge_h);

    // Title and badges share the first line; the badges flow downwards from there, and the artist
    // closes the row - see layout_rows(), which decided all of this and the row's height with it.
    const float first_center = bb.Min.y + k_row_pad_y + first_line_h * 0.5f;

    const ImVec4 title_col = playing ? theme::accent_text : theme::text_primary;
    detail::add_text_ellipsis(draw,
        ImVec2 { text_x, first_center - line_h * 0.5f },
        bb.Min.x + row.title_max_x,
        detail::to_u32(title_col),
        row.track->title.c_str());

    draw_badges(draw, row, bb.Min.x, first_center);

    const float artist_y = bb.Max.y - k_row_pad_y - line_h;
    detail::add_text_ellipsis(
        draw, ImVec2 { text_x, artist_y }, bb.Max.x - k_badge_margin, detail::to_u32(theme::text_muted), row.track->artist.c_str());

    return double_clicked;
}

void draw_track_list(const state::cache& snapshot, float height)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::surface);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 6.f, 6.f });

    if(ImGui::BeginChild("##qp_tracks", ImVec2 { 0.f, height }, ImGuiChildFlags_Borders)) {
        if(g_rows.empty()) {
            ImGui::TextUnformatted(snapshot.tracks_playlist_id.empty() ? "Pick a playlist." : "This playlist is empty.");
        }
        else {
            // Read through pending, not the raw snapshot, so a just-clicked track is marked as
            // playing while its request is still in flight (see ui/qp/qp_pending.hxx).
            const std::string_view playing_id = qp::pending::display_playing_entry(snapshot);

            // Stepped once for the whole list, not per row: there is one hover animation and one
            // selection animation in flight, whatever the row count.
            const auto dt = detail::dt_ms();
            if(!g_hover_tween.isFinished()) {
                g_hover_t = g_hover_tween.step(dt);
            }
            if(!g_select_tween.isFinished()) {
                g_select_t = g_select_tween.step(dt);
            }
            g_hover_seen_this_frame = false;

            // Relaid out when the column resizes, including the resize caused by the scrollbar
            // appearing. That cannot flap: a narrower column only ever wraps badges onto *more*
            // lines, so the content can never shrink back below the height that summoned the
            // scrollbar in the first place.
            const float content_w = ImGui::GetContentRegionAvail().x;
            if(content_w != g_rows_layout_width) {
                layout_rows(content_w);
            }

            // Rows have their own heights now, so culling is a lookup in the prefix sum rather than
            // ImGuiListClipper (whose simple mode assumes a uniform height). A thousand-entry
            // playlist still only draws what is on screen - this runs from EndScene.
            const float scroll_y = ImGui::GetScrollY();
            const float view_h = ImGui::GetWindowHeight();
            const float total_h = g_row_offsets.back();

            const auto first_it = std::upper_bound(g_row_offsets.begin(), g_row_offsets.end(), scroll_y);
            const auto first = static_cast<std::size_t>((std::max)(std::ptrdiff_t { 0 }, first_it - g_row_offsets.begin() - 1));
            const auto last_it = std::lower_bound(g_row_offsets.begin(), g_row_offsets.end(), scroll_y + view_h);
            const auto last = (std::min)(g_rows.size(), static_cast<std::size_t>(last_it - g_row_offsets.begin()) + 1);

            // Zero item spacing while the rows are placed: every gap here is baked into the heights
            // the prefix sum was built from, and ImGui's own spacing on top would drift the two apart.
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 { ImGui::GetStyle().ItemSpacing.x, 0.f });

            const float base_y = ImGui::GetCursorPosY();
            ImGui::SetCursorPosY(base_y + g_row_offsets[first]);

            for(std::size_t i = first; i < last; ++i) {
                const row_view& row = g_rows[i];
                const bool playing = !playing_id.empty() && row.track->entry_id == playing_id;

                if(draw_track_row(static_cast<int>(i), row, playing)) {
                    qp::pending::request_play(acting_playlist_id(snapshot), row.track->entry_id);
                }
            }

            // Claims the full scrollable height, so the scrollbar reflects the whole playlist and
            // not just the handful of rows that were actually drawn.
            ImGui::SetCursorPosY(base_y + total_h);
            ImGui::Dummy(ImVec2 { 0.f, 0.f });
            ImGui::PopStyleVar();

            if(!g_hover_seen_this_frame && g_hover_row >= 0) {
                g_prev_hover_row = g_hover_row;
                g_hover_row = -1;
                g_hover_t = 0.f;
                g_hover_tween = tweeny::from(0.f).to(1.f).during(k_hover_ms).via(tweeny::easing::cubicOut);
            }
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void draw_context_character_tab(const state::track& entry)
{
    const qp::character_id shown = qp::pending::display_character(entry);
    const auto characters = qp::all_characters();

    int selected = 0; // row 0 is "No override"
    for(std::size_t i = 0; i < characters.size(); ++i) {
        if(characters[i].id == shown) {
            selected = static_cast<int>(i) + 1;
            break;
        }
    }

    g_context_characters.set_selected(selected);
    g_context_characters.set_size(ImVec2 { 0.f, ImGui::GetContentRegionAvail().y });
    g_context_characters.update();

    if(!g_context_characters.selection_changed()) {
        return;
    }

    const int picked = g_context_characters.selected_index();
    if(picked == 0) {
        qp::pending::request_character(entry.entry_id, qp::character_id::none);
    }
    else if(picked > 0 && picked <= static_cast<int>(characters.size())) {
        qp::pending::request_character(entry.entry_id, characters[static_cast<std::size_t>(picked - 1)].id);
    }
}

void draw_context_tags_tab(const state::track& entry)
{
    if(ImGui::BeginChild("##qp_ctx_tags", ImVec2 { 0.f, ImGui::GetContentRegionAvail().y })) {
        for(std::size_t i = 0; i < g_selectable_tags.size(); ++i) {
            const qp::tag_id id = g_selectable_tags[i];
            const auto& definition = qp::tag(id);
            const bool enabled = qp::pending::display_tag_enabled(entry, id);

            ImGui::PushID(static_cast<int>(i));

            g_tag_toggles[i].set_checked(enabled);
            g_tag_toggles[i].update();

            const bool toggled = g_tag_toggles[i].changed();
            const bool now_enabled = g_tag_toggles[i].checked();

            // Wrapped rather than right-aligned against a fixed offset: the labels are full
            // sentences of wildly different lengths, and the first attempt let the longest ones run
            // straight under the numeric field.
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextUnformatted(definition.label.data(), definition.label.data() + definition.label.size());
            ImGui::PopTextWrapPos();

            // The parameter goes on its own indented line, not beside the label. Sharing the line
            // means either clipping the label or reserving width the short labels waste, and only
            // 7 of the 17 tags have a parameter at all.
            bool parameter_edited = false;
            if(definition.has_parameter) {
                g_tag_params[i].set_value(qp::pending::display_tag_parameter(entry, id));
                g_tag_params[i].set_width(k_context_param_w);
                ImGui::Indent(28.f);
                g_tag_params[i].update();
                ImGui::Unindent(28.f);
                parameter_edited = g_tag_params[i].changed();
            }

            ImGui::PopID();

            // A parameter edit on an already-enabled tag is a change to that tag, so it goes out as
            // the same request - the host rebuilds the whole tag set from what it is told either way.
            if(toggled || (parameter_edited && now_enabled)) {
                qp::pending::request_tag(entry.entry_id, id, now_enabled, g_tag_params[i].value(), definition.has_parameter);
            }
        }
    }
    ImGui::EndChild();
}

void draw_context_mods_tab(const state::track& entry)
{
    const auto& ids = tw::ui::overlay_state::all_tweak_ids();
    for(std::size_t i = 0; i < ids.size(); ++i) {
        g_mod_toggles[i].set_checked(qp::pending::display_mod_enabled(entry, ids[i]));
        g_mod_toggles[i].update();

        if(g_mod_toggles[i].changed()) {
            qp::pending::request_mod(entry.entry_id, ids[i], g_mod_toggles[i].checked());
        }

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        const std::string_view name = tw::ui::overlay_state::tweak_display_name(ids[i]);
        ImGui::TextUnformatted(name.data(), name.data() + name.size());
    }
}

// Drawn outside the track list's child window and re-resolved from the snapshot every frame: the
// row vector is rebuilt whenever the host echoes an edit back, so a pointer captured when the menu
// opened would dangle on the very first change the menu itself caused.
void draw_track_context_menu(const state::cache& snapshot)
{
    if(!g_context_popup.opened()) {
        return;
    }

    const state::track* entry = state::find_track(snapshot, g_context_entry_id);
    if(entry == nullptr) {
        g_context_popup.close();
        return;
    }

    g_context_popup.set_title(entry->title);
    g_context_popup.begin();

    g_context_tabs.set_size(k_context_size);
    g_context_tabs.begin();
    if(g_context_tabs.begin_view(0)) {
        draw_context_character_tab(*entry);
        g_context_tabs.end_view();
    }
    if(g_context_tabs.begin_view(1)) {
        draw_context_tags_tab(*entry);
        g_context_tabs.end_view();
    }
    if(g_context_tabs.begin_view(2)) {
        draw_context_mods_tab(*entry);
        g_context_tabs.end_view();
    }
    g_context_tabs.end();

    g_context_popup.end();
}

// One full-width summary button reading "Character: <name>", matching the desktop's collapsed
// character band. The first pass put a fixed-offset label beside the button and the label ran
// straight into it (Fuckups/playback_control_template_miss.png) - the offset was guessed rather than
// measured, and "Default character" is wider than the guess at this font size. Folding the label
// into the button's own text removes the measurement from the problem entirely.
void draw_character_row(const state::cache& snapshot)
{
    const qp::character_id shown = qp::pending::display_default_character(snapshot);
    const std::string_view current = qp::character_display_name(shown);

    std::string label = "Character: ";
    label += current.empty() ? "(unset)" : current;

    g_character_btn.set_size(ImVec2 { ImGui::GetContentRegionAvail().x, k_char_row_h });
    g_character_btn.update(label.c_str());
    if(g_character_btn.clicked()) {
        g_character_popup.open();
    }

    if(g_character_popup.opened()) {
        g_character_popup.begin();
        const auto characters = qp::all_characters();
        for(std::size_t i = 0; i < characters.size() && i < g_character_rows.size(); ++i) {
            g_character_rows[i].set_content(list_item_content { .text = std::string(characters[i].display_name) });
            g_character_rows[i].set_selected(characters[i].id == shown);
            g_character_rows[i].update();
            if(g_character_rows[i].clicked()) {
                qp::pending::request_default_character(characters[i].id);
                g_character_popup.close();
            }
        }
        g_character_popup.end();
    }
}

void draw_transport_buttons(const state::cache& snapshot)
{
    // What is playing right now, as the host last reported it - the baseline a next/prev request is
    // confirmed against (see request_transport).
    const std::string_view playing_id = snapshot.playing_entry_id;

    g_prev_btn.update();
    if(g_prev_btn.clicked()) {
        qp::pending::request_transport("prev", playing_id);
    }

    ImGui::SameLine(0.f, 6.f);
    g_play_btn.update();
    if(g_play_btn.clicked()) {
        // Play acts on the highlighted row; with nothing highlighted it starts the playlist from
        // the top, which is what the desktop's own Play does (SelectedTrackCard ?? first).
        const int index = g_selected_track >= 0 ? g_selected_track : 0;
        if(index < static_cast<int>(g_rows.size())) {
            const std::string_view playlist_id = acting_playlist_id(snapshot);
            qp::pending::request_play(playlist_id, g_rows[static_cast<std::size_t>(index)].track->entry_id);
        }
    }

    ImGui::SameLine(0.f, 6.f);
    g_stop_btn.update();
    if(g_stop_btn.clicked()) {
        qp::pending::request_transport("stop", playing_id);
    }

    ImGui::SameLine(0.f, 6.f);
    g_next_btn.update();
    if(g_next_btn.clicked()) {
        qp::pending::request_transport("next", playing_id);
    }
}

// Transport, MODE and PLAYBACK on one line, separated by rules, with the leftover width spread
// evenly between the groups - the same shape (and the same reasoning) as the desktop's transport bar
// after its 2026-08-13 mockup pass: three stacked centred rows with captions above them cost two
// rows of height that the track list wanted. Captions are inline micro-caps beside their group.
//
// The one deliberate divergence: the six playback chips stay words here, where the desktop had to
// go to glyphs. That was forced by width - six text chips need ~950px and the desktop's bar has
// ~656px at its minimum window size. This tab grows the overlay window to its content instead, so
// the constraint does not apply, and words are strictly more readable than a glyph family.
void draw_transport_row(const state::cache& snapshot)
{
    const std::string_view playlist_id = acting_playlist_id(snapshot);
    const auto modes = qp::all_playback_modes();

    // Pushed in from the snapshot every frame; set_selected is a no-op when unchanged and never
    // raises changed(), so echoing host state back cannot loop against the user's own click. Read
    // through pending so the chip a click just moved stays moved until the host confirms it - or
    // snaps back on its own when the confirmation never arrives.
    const qp::playback_mode shown_mode = qp::pending::display_mode(snapshot);
    for(std::size_t i = 0; i < modes.size(); ++i) {
        if(modes[i].id == shown_mode) {
            g_modes.set_selected(static_cast<int>(i));
            break;
        }
    }
    g_advance.set_selected(qp::pending::display_advance(snapshot) == qp::advance_trigger::manual ? 1 : 0);

    constexpr const char* k_mode_caption = "MODE";
    constexpr const char* k_playback_caption = "PLAYBACK";
    constexpr float k_btn_gap = 6.f;

    const float buttons_w = k_transport_btn * 4.f + k_btn_gap * 3.f;
    const float mode_caption_w = ImGui::CalcTextSize(k_mode_caption).x;
    const float playback_caption_w = ImGui::CalcTextSize(k_playback_caption).x;
    const float advance_w = g_advance.width();
    const float modes_w = g_modes.width();
    const float divider_w = k_divider_w + k_divider_margin * 2.f;

    const float content_w = buttons_w + divider_w * 2.f + mode_caption_w + k_group_gap + advance_w + playback_caption_w + k_group_gap
                            + modes_w;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float avail_w = ImGui::GetContentRegionAvail().x;

    // Four gaps between the five groups, exactly like the desktop bar's four star columns: left to
    // itself the row packs against the left edge and pools every spare pixel at the right. The first
    // version divided the slack by four but only spent it at the two dividers, so half of it stayed
    // pooled on the right anyway - the symptom that made this look like it was not distributing at
    // all.
    const float slack = (std::max)(0.f, avail_w - content_w) / 4.f;
    const float text_h = ImGui::GetFontSize();

    float x = origin.x;
    const auto place = [&](float height) {
        ImGui::SetCursorScreenPos(ImVec2 { x, origin.y + (k_transport_btn - height) * 0.5f });
    };

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const auto divider = [&]() {
        x += slack + k_divider_margin;
        draw->AddLine(ImVec2 { x, origin.y + 4.f },
            ImVec2 { x, origin.y + k_transport_btn - 4.f },
            detail::to_u32(theme::border_divider),
            k_divider_w);
        x += k_divider_w + k_divider_margin + slack;
    };

    place(k_transport_btn);
    draw_transport_buttons(snapshot);
    x += buttons_w;

    divider();

    place(text_h);
    ImGui::TextUnformatted(k_mode_caption);
    x += mode_caption_w + k_group_gap;

    place(k_strip_h);
    g_advance.update();
    if(g_advance.changed() && !playlist_id.empty()) {
        qp::pending::request_advance(playlist_id, g_advance.selected() == 1 ? qp::advance_trigger::manual : qp::advance_trigger::automatic);
    }
    x += advance_w;

    divider();

    place(text_h);
    ImGui::TextUnformatted(k_playback_caption);
    x += playback_caption_w + k_group_gap;

    place(k_strip_h);
    g_modes.update();
    if(g_modes.hovered_index() >= 0) {
        // Name first, then the explanation - the chips are glyphs now, so the tooltip is the only
        // place the mode is named at all. Same shape as the desktop's own Tooltip property.
        // Length-bounded rather than %s + data(): these views happen to be literals today, but a
        // hint assembled as a substring later would run past its end here and nowhere else.
        const auto& hovered = modes[static_cast<std::size_t>(g_modes.hovered_index())];
        ImGui::SetTooltip("%.*s\n%.*s",
            static_cast<int>(hovered.display_name.size()),
            hovered.display_name.data(),
            static_cast<int>(hovered.hint.size()),
            hovered.hint.data());
    }
    if(g_modes.changed() && !playlist_id.empty()) {
        qp::pending::request_mode(playlist_id, modes[static_cast<std::size_t>(g_modes.selected())].id);
    }

    // Everything above positioned itself absolutely, so the parent layout has no idea how much room
    // the row took. Claim it explicitly, or the next widget lands on top of the transport.
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2 { avail_w, k_transport_btn });
}
} // namespace

namespace tw::ui::plugins::interactive::player
{
void initialize() noexcept
{
    g_widgets_ready = false;
    g_seen_catalog = nullptr;
    g_rows_source.reset();
    g_rows.clear();
    g_playlist_ids.clear();
    g_selected_track = -1;
    g_requested_playlist_id.clear();
    g_context_entry_id.clear();

    g_hover_row = -1;
    g_prev_hover_row = -1;
    g_prev_selected_row = -1;
    g_hover_t = 1.f;
    g_select_t = 1.f;
}

void shutdown() noexcept
{
    g_rows_source.reset();
    g_rows.clear();
}

ImVec2 desired_content_size(const tw::ui::qp::state::cache& /*snapshot*/)
{
    ensure_widgets_ready();

    // Measured from font metrics rather than from a previous frame: the window has to be grown on
    // the very frame this tab is first opened, and there is no earlier frame of it to measure.
    //
    // The transport row holds both strips plus the buttons, the captions and two rules, so it - not
    // the playback strip alone - is what the tab has to be wide enough for. Measured off the widgets
    // themselves rather than re-derived here: two copies of the layout arithmetic would drift, and
    // this number is also what stops the window being dragged narrower than the row (menu.cxx).
    const float row_w = k_transport_btn * 4.f + 6.f * 3.f + (k_divider_w + k_divider_margin * 2.f) * 2.f
                        + ImGui::CalcTextSize("MODE").x + k_group_gap + g_advance.width() + ImGui::CalcTextSize("PLAYBACK").x
                        + k_group_gap + g_modes.width();

    const float width = k_playlist_col_w + k_col_gap + (std::max)(k_min_track_col_w, row_w);

    return ImVec2 { width, k_min_body_h + footer_height() };
}

void draw(const tw::ui::qp::state::cache& snapshot)
{
    ensure_widgets_ready();

    if(!snapshot.connected) {
        ImGui::TextUnformatted("Quick Player is not connected.");
        ImGui::TextUnformatted("Playlists appear here once the desktop app is running.");
        return;
    }

    sync_playlists(snapshot);
    sync_rows(snapshot);

    // Nothing open yet: adopt whatever the host says it is positioned in, so the tab has content on
    // its first frame instead of waiting for a click it should not need.
    if(g_requested_playlist_id.empty() && snapshot.tracks_playlist_id.empty() && !snapshot.order.playlist_id.empty()) {
        request_playlist(snapshot.order.playlist_id);
    }

    // The open playlist is the host's answer, not the list's own click history - pushing it back in
    // is what keeps the left column highlighted after the tab adopts a playlist by itself above.
    const std::string_view open_id = acting_playlist_id(snapshot);
    for(std::size_t i = 0; i < g_playlist_ids.size(); ++i) {
        if(g_playlist_ids[i] == open_id) {
            g_playlists.set_selected(static_cast<int>(i));
            break;
        }
    }

    // No lower clamp beyond a token minimum: clamping the body to a comfortable height is what
    // pushed the footer past the bottom of the tab and produced the permanent scrollbar. If the
    // window is genuinely too short, the list gets small - it does not get a scrollbar around
    // everything.
    const float body_h = (std::max)(40.f, ImGui::GetContentRegionAvail().y - footer_height());

    g_playlists.set_size(ImVec2 { k_playlist_col_w, body_h });
    g_playlists.update();
    if(g_playlists.selection_changed()) {
        const int index = g_playlists.selected_index();
        if(index >= 0 && index < static_cast<int>(g_playlist_ids.size())) {
            request_playlist(g_playlist_ids[static_cast<std::size_t>(index)]);
        }
    }

    // The open playlist's name over its tracks, the way the desktop's queue card is titled - without
    // it the right-hand column never says which playlist you are looking at, and the left column's
    // highlight is easy to lose track of.
    ImGui::SameLine(0.f, k_col_gap);
    ImGui::BeginGroup();

    const float header_h = ImGui::GetFontSize();
    std::string_view open_name;
    for(std::size_t i = 0; i < g_playlist_ids.size(); ++i) {
        if(g_playlist_ids[i] == open_id) {
            open_name = state::playlists_of(snapshot)[i].name;
            break;
        }
    }

    const float header_w = ImGui::GetContentRegionAvail().x;
    if(!open_name.empty()) {
        // Sampled here rather than above the branch: the cursor does not move until the Dummy below.
        const ImVec2 header_pos = ImGui::GetCursorScreenPos();
        detail::add_text_ellipsis(ImGui::GetWindowDrawList(),
            header_pos,
            header_pos.x + header_w,
            detail::to_u32(theme::text_primary),
            std::string(open_name).c_str());
    }
    ImGui::Dummy(ImVec2 { header_w, header_h });

    draw_track_list(snapshot, body_h - header_h - ImGui::GetStyle().ItemSpacing.y);
    ImGui::EndGroup();

    // Outside the list's child window on purpose: the popup is its own ImGui window, and keeping it
    // out of the clipped child means the scroll position cannot drag it around.
    draw_track_context_menu(snapshot);

    // No spacer Dummies between the footer rows: ImGui already puts ItemSpacing.y between items, and
    // adding gaps on top of it is exactly what made footer_height() disagree with reality.
    draw_character_row(snapshot);
    draw_transport_row(snapshot);
}
} // namespace tw::ui::plugins::interactive::player
