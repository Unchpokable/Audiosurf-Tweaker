// No TweakerPlugin PCH here - shared with smoke_test, same convention as texture_cache.cxx.
// No windows.h either: the Insert hotkey is no longer polled from here, it arrives as a WndProc
// message from whichever harness owns the message pump (see menu.hxx, toggle_visible()).
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <span>
#include <string>
#include <vector>

#include "ui/plugins/interactive/menu.hxx"

#include "ui/kit/anim.hxx"
#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"

#include "ui/overlay_config.hxx"

#include "ui/plugins/interactive/player.hxx"

#include "ui/pending_actions.hxx"

#include "ui/theme.hxx"

#include "ui/widgets/button.hxx"
#include "ui/widgets/color_field.hxx"
#include "ui/widgets/item_group.hxx"
#include "ui/widgets/list_view.hxx"
#include "ui/widgets/segmented.hxx"
#include "ui/widgets/tab_view.hxx"
#include "ui/widgets/toggle.hxx"

namespace
{
using tw::ui::widgets::button;
using tw::ui::widgets::color_field;
using tw::ui::widgets::item_group;
using tw::ui::widgets::list_item_content;
using tw::ui::widgets::list_view;
using tw::ui::widgets::segmented;
using tw::ui::widgets::tab_view;
using tw::ui::widgets::toggle;

namespace theme = tw::ui::theme;
namespace paint = tw::ui::kit::paint;

// Live theme color editor (Settings tab) - one row per tw::ui::theme field, grouped into
// item_group boxes matching the section comments in theme.hxx.
struct theme_color_entry {
    const char* key;   // theme.hxx field name -> stable widget id
    const char* label; // display text next to the swatch
    ImVec4* color;     // direct pointer into tw::ui::theme - edits apply live
};

// The same color_field the Skybox palette uses (review U9): a left click on the swatch opens the picker
// under it, and the theme is saved once per gesture rather than on every frame of a drag.
struct theme_color_row {
    const theme_color_entry* entry;
    color_field field;

    explicit theme_color_row(const theme_color_entry& e) : entry(&e), field((std::string("menu_theme_") + e.key).c_str())
    {
        field.set_label(e.label);
        field.set_swatch_first(true);
    }
};

// Sentinel last_size.x used before a theme_group has ever been measured - see theme_group below.
constexpr float k_theme_group_size_unmeasured = 1.0e6f;

struct theme_group {
    item_group group;
    std::vector<theme_color_row> rows;
    // Measured footprint from the last time this group was drawn (GetItemRectSize() right after
    // group.end()) - masonry layout below packs this frame's columns using last frame's sizes,
    // since item_group is shrink-to-content and only knows its own size after end(). Seeded huge
    // so the very first draw falls back to a single column (self-corrects to the real layout next
    // frame) instead of guessing zero width and cramming everything into one column.
    ImVec2 last_size { k_theme_group_size_unmeasured, 0.f };

    theme_group(const char* id, const char* title, std::span<const theme_color_entry> entries) : group(id, title)
    {
        rows.reserve(entries.size());
        for(const auto& e : entries) {
            rows.emplace_back(e);
        }
    }
};

const theme_color_entry k_accent_colors[] = {
    { "accent_primary", "Accent Primary", &theme::accent_primary },
    { "accent_secondary", "Accent Secondary", &theme::accent_secondary },
    { "accent_text", "Accent Text", &theme::accent_text },
    { "accent_selection", "Accent Selection", &theme::accent_selection },
    { "accent_soft", "Accent Soft", &theme::accent_soft },
    { "accent_hover", "Accent Hover", &theme::accent_hover },
    { "accent_pressed", "Accent Pressed", &theme::accent_pressed },
    { "accent_border", "Accent Border", &theme::accent_border },
    { "accent_selected", "Accent Selected", &theme::accent_selected },
    { "accent_ghost", "Accent Ghost", &theme::accent_ghost },
};

const theme_color_entry k_surface_colors[] = {
    { "surface", "Surface", &theme::surface },
    { "surface_muted", "Surface Muted", &theme::surface_muted },
    { "surface_soft", "Surface Soft", &theme::surface_soft },
    { "surface_hover", "Surface Hover", &theme::surface_hover },
    { "surface_input", "Surface Input", &theme::surface_input },
    { "surface_row", "Surface Row", &theme::surface_row },
    { "surface_row_hover", "Surface Row Hover", &theme::surface_row_hover },
    { "surface_skeleton", "Surface Skeleton", &theme::surface_skeleton },
    { "surface_badge", "Surface Badge", &theme::surface_badge },
    { "surface_elevated", "Surface Elevated", &theme::surface_elevated },
    { "control_track_off", "Control Track Off", &theme::control_track_off },
    { "control_thumb", "Control Thumb", &theme::control_thumb },
};

const theme_color_entry k_border_colors[] = {
    { "border", "Border", &theme::border },
    { "border_subtle", "Border Subtle", &theme::border_subtle },
    { "border_strong", "Border Strong", &theme::border_strong },
    { "border_divider", "Border Divider", &theme::border_divider },
    { "border_window", "Border Window", &theme::border_window },
};

const theme_color_entry k_text_colors[] = {
    { "text_primary", "Text Primary", &theme::text_primary },
    { "text_secondary", "Text Secondary", &theme::text_secondary },
    { "text_muted", "Text Muted", &theme::text_muted },
    { "text_subtle", "Text Subtle", &theme::text_subtle },
    { "text_faint", "Text Faint", &theme::text_faint },
    { "text_glyph_muted", "Text Glyph Muted", &theme::text_glyph_muted },
    { "text_on_accent", "Text On Accent", &theme::text_on_accent },
};

constexpr float k_title_h = tw::ui::theme::space::title_h;
constexpr float k_padding = tw::ui::theme::space::window_pad;
constexpr float k_theme_group_gap = 12.f; // horizontal gap between theme item_groups sharing a row
constexpr ImVec2 k_min_size { 320.f, 260.f };
constexpr const char* k_title = "Audiosurf Tweaker";

// Index of the Player tab in the label array below - referenced from three places, and an inline 2
// in any of them would silently become the wrong tab the moment a tab is inserted before it.
constexpr int k_player_tab = 2;

// What the tab_view's own nav column and the window chrome take off the top before any tab gets to
// use the space. Approximate on purpose: it feeds the one-shot window grow below, whose job is
// "make the Player tab usable", not pixel-exact fitting.
constexpr float k_chrome_allowance_x = 96.f;

// Written from the message-pump thread (toggle_visible/set_visible), read from the render thread
// (update) and from the dinput hooks on whichever thread the game polls its devices.
std::atomic<bool> g_visible = false;

// Tabs contributed from outside tweaker_ui - see menu.hxx::add_extra_tab. Empty in smoke_test, where
// the modules that register them do not exist.
//
// A list rather than the single slot this used to be: Skybox is no longer the only page owned by a
// module this library cannot link against. They are appended after the four built-in tabs in
// registration order, and the handle add_extra_tab returns is that position.
constexpr int k_first_extra_tab = 4;

struct extra_tab {
    std::string label;
    tw::ui::plugins::interactive::menu::extra_tab_draw_fn draw;
};

std::vector<extra_tab> g_extra_tabs;

tab_view g_tabs { "menu_tabs" };
list_view g_skins_list { "menu_skins" };
button g_skins_apply_btn { "menu_skins_apply", { 100.f, 28.f } };
std::vector<toggle> g_tweak_toggles;
bool g_widgets_ready = false;

ImVec2 g_pos { -1.f, -1.f }; // sentinel: pick a centered default on first show
ImVec2 g_size { 420.f, 520.f };

// Smallest size the *currently selected* tab can live with, recomputed each frame. Tab content does
// not reflow: the Player tab's transport row is a fixed-width arrangement of buttons, rules and
// glyph strips, so a window narrower than it does not compact - the row simply runs off the edge.
// Clamping the resize grip instead is the honest answer, and it is per-tab because the other three
// are perfectly usable at k_min_size.
ImVec2 g_active_min_size = k_min_size;

// Which tab was selected last frame, so entering the Player tab can be detected as an edge rather
// than re-applied on every frame it stays open.
int g_last_tab = 0;

bool g_dragging_move = false;
bool g_dragging_resize = false;
ImVec2 g_drag_start_mouse {};
ImVec2 g_drag_start_value {};

std::vector<std::string> g_last_skin_names;

std::vector<theme_group> g_theme_groups;

segmented g_feed_side { "menu_feed_side" };
segmented g_pins_side { "menu_pins_side" };
button g_theme_reset_btn { "menu_theme_reset", { 100.f, 28.f } };
toggle g_offline_pin_toggle { "menu_offline_pin" };

void ensure_widgets_ready()
{
    if(g_widgets_ready) {
        return;
    }

    // The extra tab is appended, never inserted: k_player_tab and the literal indices in update()
    // are positional, and anything that shifted them would silently draw the wrong page.
    std::vector<std::string_view> labels { "Skins", "Tweaks", "Player", "Settings" };
    for(const extra_tab& tab : g_extra_tabs) {
        labels.emplace_back(tab.label);
    }

    g_tabs.set_tabs(labels);
    g_tabs.set_rounding(tw::ui::theme::radius::panel);

    static const std::string_view k_side_labels[] = { "Left", "Right" };
    g_feed_side.set_options(k_side_labels);
    g_pins_side.set_options(k_side_labels);

    g_tweak_toggles.clear();
    g_tweak_toggles.reserve(tw::ui::overlay_state::k_tweak_count);
    for(std::size_t i = 0; i < tw::ui::overlay_state::k_tweak_count; ++i) {
        const std::string id = "menu_tweak_" + std::to_string(i);
        g_tweak_toggles.emplace_back(id.c_str());
    }

    if(g_theme_groups.empty()) {
        g_theme_groups.reserve(4);
        g_theme_groups.emplace_back("menu_theme_group_accent", "Accent", std::span { k_accent_colors });
        g_theme_groups.emplace_back("menu_theme_group_surfaces", "Surfaces", std::span { k_surface_colors });
        g_theme_groups.emplace_back("menu_theme_group_borders", "Borders", std::span { k_border_colors });
        g_theme_groups.emplace_back("menu_theme_group_text", "Text", std::span { k_text_colors });
    }

    g_pos = tw::ui::overlay_config::menu_pos();
    g_size = tw::ui::overlay_config::menu_size();

    g_widgets_ready = true;
}

// What the host-backed tabs (Skins, Tweaks, Player) show while Audiosurf Tweaker is not connected. The tabs
// stay where they are - indices are positional, and a tab strip that reshuffles itself on every
// connect/disconnect is worse than a page that explains itself (Docs/Internal/plugin-offline-mode.md, Р-4).
// With nothing to click, no pending request can be started against a host that is not there.
void draw_offline_stub()
{
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
    ImGui::TextWrapped("Audiosurf Tweaker is not connected.");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("Skins, tweaks and Quick Player need Audiosurf Tweaker running. Skyboxes and scripts work without it.");
    ImGui::PopStyleColor();
}

void draw_skins_tab(const tw::ui::overlay_state::cache& snapshot)
{
    if(snapshot.skin_names != g_last_skin_names) {
        std::vector<list_item_content> items;
        items.reserve(snapshot.skin_names.size());
        for(const auto& name : snapshot.skin_names) {
            items.emplace_back(name);
        }
        g_skins_list.set_items(items);
        g_last_skin_names = snapshot.skin_names;
    }

    const std::string_view display_skin = tw::ui::pending_actions::skin_display_name(snapshot);
    if(display_skin.empty()) {
        ImGui::TextUnformatted("Current skin: (none)");
    }
    else {
        ImGui::Text("Current skin: %.*s", static_cast<int>(display_skin.size()), display_skin.data());
    }
    ImGui::Spacing();

    // Clicking a row only moves the local highlight - it does NOT send NOTIFY_SKIN by itself.
    // Installing a skin unpacks files on the host, unlike a tweak toggle, so it's gated behind the
    // explicit Apply button below (a couple of accidental/rapid clicks in the list shouldn't be
    // able to queue several installs back to back).
    const float list_h = (std::max)(80.f, ImGui::GetContentRegionAvail().y - 40.f);
    g_skins_list.set_size(ImVec2 { 0.f, list_h });
    g_skins_list.update();

    ImGui::Spacing();

    const int selected = g_skins_list.selected_index();
    const bool has_selection = selected >= 0 && static_cast<std::size_t>(selected) < g_last_skin_names.size();
    const std::string_view selected_name =
        has_selection ? std::string_view { g_last_skin_names[static_cast<std::size_t>(selected)] } : std::string_view {};
    const bool already_current = has_selection && selected_name == display_skin;
    const bool pending = tw::ui::pending_actions::has_pending_skin();
    const bool can_apply = has_selection && !already_current && !pending;

    // Disabled rather than ignoring the click (review R12): nothing picked, the current skin picked, or an
    // install already on its way.
    g_skins_apply_btn.set_enabled(can_apply);
    g_skins_apply_btn.update(pending ? "Applying..." : "Apply");
    if(g_skins_apply_btn.clicked() && can_apply) {
        tw::ui::pending_actions::request_skin(selected_name);
    }
}

void draw_tweaks_tab(const tw::ui::overlay_state::cache& snapshot)
{
    const auto& ids = tw::ui::overlay_state::all_tweak_ids();
    for(std::size_t i = 0; i < ids.size(); ++i) {
        const bool enabled = tw::ui::pending_actions::tweak_display_enabled(snapshot, ids[i]);
        g_tweak_toggles[i].set_checked(enabled); // no-op if unchanged - see toggle::set_checked

        const ImRect label = paint::label_row(tw::ui::overlay_state::tweak_display_name(ids[i]).data(), g_tweak_toggles[i].size());
        if(tw::ui::overlay_state::is_tweak_quick_player(snapshot, ids[i])) {
            ImGui::GetWindowDrawList()->AddText(
                ImVec2 { label.Max.x + ImGui::GetStyle().ItemSpacing.x, label.Min.y }, paint::to_u32(theme::accent_secondary), "QP");
        }
        g_tweak_toggles[i].update();
        if(g_tweak_toggles[i].changed()) {
            // Cheap idempotent write (unlike a skin install) - fires straight from the click, no
            // Apply gate needed. See ui/pending_actions.hxx for the confirm/timeout/rollback logic.
            tw::ui::pending_actions::request_tweak(ids[i], g_tweak_toggles[i].checked());
        }
    }
}

// A side as a two-chip strip (review U9) rather than two buttons with "[*]" on the current one.
void draw_side_row(const char* label, tw::ui::overlay_config::side current, segmented& strip, void (*setter)(tw::ui::overlay_config::side))
{
    using tw::ui::overlay_config::side;

    strip.set_selected(current == side::right ? 1 : 0); // no-op if unchanged, never raises changed()
    paint::label_row(label, ImVec2 { strip.width(), strip.height() });
    strip.update();
    if(strip.changed()) {
        setter(strip.selected() == 1 ? side::right : side::left);
        tw::ui::overlay_config::request_save();
    }
}

// Serializes the live (possibly edited) theme and marks the config for saving - called once per colour
// gesture (color_field::committed) and after a Default reset, mirroring how draw_side_row saves right after
// each setter.
void persist_theme()
{
    tw::ui::overlay_config::set_theme_overrides(tw::ui::theme::to_config());
    tw::ui::overlay_config::request_save();
}

// One theme colour. Applied on every change - the overlay recolours under the drag - and saved when the
// gesture ends.
void draw_theme_color_row(theme_color_row& row)
{
    row.field.set_color(*row.entry->color); // ignored while the picker is open
    row.field.update();
    if(row.field.changed()) {
        *row.entry->color = row.field.color();
    }
    if(row.field.committed()) {
        persist_theme();
    }
}

// Masonry (shortest-column-first) layout: unlike a row-based flow, this lets a short group (e.g.
// Borders) share a column with the next group (e.g. Text) instead of leaving dead space under it
// while a taller neighbor (e.g. Surfaces) pushes the next row down past everything. Column
// count/widths and each group's column assignment are all decided from sizes measured last frame
// (theme_group::last_size) - item_group only knows its own size after end(), so this is
// necessarily one frame behind; harmless since the groups' content never changes at runtime.
void draw_theme_groups()
{
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const ImVec2 start = ImGui::GetCursorPos();
    const std::size_t group_count = g_theme_groups.size();
    if(group_count == 0) {
        return;
    }

    float max_w = 0.f;
    bool unmeasured = false;
    for(const theme_group& group : g_theme_groups) {
        max_w = (std::max)(max_w, group.last_size.x);
        if(group.last_size.x >= k_theme_group_size_unmeasured) {
            unmeasured = true;
        }
    }

    // Bootstrap: fall back to a single column until every group has a real measured size (see
    // theme_group::last_size) - self-corrects to the real layout on the following frame.
    const std::size_t column_count =
        unmeasured ? std::size_t { 1 }
                   : std::clamp<std::size_t>(
                         static_cast<std::size_t>((avail_w + k_theme_group_gap) / (max_w + k_theme_group_gap)), 1, group_count);

    // Pass 1 - simulate placement on cached sizes only (no drawing): assign each group, in its
    // existing fixed order, to whichever column currently has the least accumulated height.
    std::vector<std::size_t> column_of(group_count, 0);
    std::vector<float> column_h(column_count, 0.f);
    std::vector<float> column_w(column_count, 0.f);
    for(std::size_t g = 0; g < group_count; ++g) {
        std::size_t best = 0;
        for(std::size_t c = 1; c < column_count; ++c) {
            if(column_h[c] < column_h[best]) {
                best = c;
            }
        }
        column_of[g] = best;
        column_h[best] += g_theme_groups[g].last_size.y + k_theme_group_gap;
        column_w[best] = (std::max)(column_w[best], g_theme_groups[g].last_size.x);
    }

    // Leftover width is not given to the boxes themselves - it is spread as extra margin between
    // columns (space-between style) so a full set of columns still spans edge to edge.
    float total_w = 0.f;
    for(const float w : column_w) {
        total_w += w;
    }
    const float extra = (std::max)(0.f, avail_w - total_w - k_theme_group_gap * static_cast<float>(column_count - 1));
    const float gap = column_count > 1 ? k_theme_group_gap + extra / static_cast<float>(column_count - 1) : k_theme_group_gap;

    std::vector<float> column_x(column_count, start.x);
    for(std::size_t c = 1; c < column_count; ++c) {
        column_x[c] = column_x[c - 1] + column_w[c - 1] + gap;
    }

    // Pass 2 - render using the column assignment from pass 1, tracking each column's own cursor_y
    // independently so a short group's column can pick up the very next group beneath it.
    std::vector<float> column_y(column_count, start.y);
    for(std::size_t g = 0; g < group_count; ++g) {
        theme_group& group = g_theme_groups[g];
        const std::size_t c = column_of[g];

        ImGui::SetCursorPos(ImVec2 { column_x[c], column_y[c] });
        group.group.begin();
        for(theme_color_row& row : group.rows) {
            draw_theme_color_row(row);
        }
        group.group.end();

        group.last_size = ImGui::GetItemRectSize(); // re-measured for next frame's packing
        column_y[c] += group.last_size.y + k_theme_group_gap;
    }
}

// Window size a tab's content needs, chrome included and clamped to the viewport - the window can
// never be asked to exceed the screen, whatever the content wants.
ImVec2 window_size_for(ImVec2 content)
{
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    return ImVec2 {
        (std::min)(viewport.x, content.x + k_chrome_allowance_x + k_padding * 2.f),
        (std::min)(viewport.y, content.y + k_title_h + k_padding * 2.f),
    };
}

// Grows the menu to fit a tab that needs more room than the current size gives it. Never shrinks:
// the size is the user's, saved in overlay_config, and a tab that pulled it back in on every switch
// would undo every resize.
void grow_to_fit(ImVec2 wanted)
{
    const ImVec2 grown { (std::max)(g_size.x, wanted.x), (std::max)(g_size.y, wanted.y) };
    if(grown.x == g_size.x && grown.y == g_size.y) {
        return;
    }

    g_size = grown;
    tw::ui::overlay_config::set_menu_size(g_size);
    tw::ui::overlay_config::request_save();
}

void draw_settings_tab()
{
    draw_side_row("Notefeed / watermark corner", tw::ui::overlay_config::feed_side(), g_feed_side, &tw::ui::overlay_config::set_feed_side);
    ImGui::Spacing();
    draw_side_row("Pins side", tw::ui::overlay_config::pins_side(), g_pins_side, &tw::ui::overlay_config::set_pins_side);
    ImGui::Spacing();

    g_offline_pin_toggle.set_checked(tw::ui::overlay_config::offline_pin()); // no-op if unchanged
    paint::label_row("Show \"Offline\" pin", g_offline_pin_toggle.size());
    g_offline_pin_toggle.update();
    if(g_offline_pin_toggle.changed()) {
        tw::ui::overlay_config::set_offline_pin(g_offline_pin_toggle.checked());
        tw::ui::overlay_config::request_save();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    paint::label_row("Color theme", g_theme_reset_btn.size());
    g_theme_reset_btn.update("Default");
    if(g_theme_reset_btn.clicked()) {
        tw::ui::theme::apply_dark();
        persist_theme();
    }
    ImGui::Spacing();

    draw_theme_groups();
}
} // namespace

namespace tw::ui::plugins::interactive::menu
{
void initialize() noexcept
{
    g_widgets_ready = false;
    g_visible.store(false, std::memory_order_relaxed);
    g_last_skin_names.clear();
    g_last_tab = 0;
    player::initialize();
}

void shutdown() noexcept
{
    g_visible.store(false, std::memory_order_relaxed);
    player::shutdown();
}

bool is_visible() noexcept
{
    return g_visible.load(std::memory_order_relaxed);
}

void toggle_visible() noexcept
{
    // Only ever called from the message-pump thread, so the read-modify-write needs no CAS loop -
    // the atomic is there for the cross-thread *reads* (render thread, dinput hooks), not for
    // concurrent writers.
    g_visible.store(!g_visible.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void set_visible(bool visible) noexcept
{
    g_visible.store(visible, std::memory_order_relaxed);
}

void show_tab(int index) noexcept
{
    // ensure_widgets_ready() first: the tab labels have to exist before a tab can be selected, and
    // this can be called before the first frame ever drew.
    ensure_widgets_ready();
    g_tabs.set_selected_tab(index);
    g_visible.store(true, std::memory_order_relaxed);
}

int add_extra_tab(std::string_view label, extra_tab_draw_fn draw) noexcept
{
    if(draw == nullptr) {
        return -1;
    }

    g_extra_tabs.push_back(extra_tab { std::string { label }, draw });
    return k_first_extra_tab + static_cast<int>(g_extra_tabs.size()) - 1;
}

bool window_rect(ImVec2& pos, ImVec2& size) noexcept
{
    if(!g_visible.load(std::memory_order_relaxed)) {
        return false;
    }

    // The sentinel means "never shown, position not chosen yet" - update() resolves it to a centered
    // default on the frame it first draws. Reporting it verbatim would dock a panel off-screen.
    if(g_pos.x < 0.f || g_pos.y < 0.f) {
        return false;
    }

    pos = g_pos;
    size = g_size;
    return true;
}

bool extra_tab_selected(int handle) noexcept
{
    return handle >= 0 && g_tabs.selected_tab() == handle;
}

void update(const tw::ui::overlay_state::cache& snapshot, const tw::ui::qp::state::cache& qp_snapshot) noexcept
{
    ensure_widgets_ready();

    // Settings edited through this menu are written to disk lazily: every mutation marks the
    // config dirty, and the actual file write happens here, on the first frame where no mouse
    // button is held. Dragging a color picker or the resize grip therefore costs one write on
    // release instead of one write per frame while dragging. See ui/overlay_config.hxx.
    if(!ImGui::IsAnyMouseDown()) {
        tw::ui::overlay_config::flush();
    }

    if(!g_visible.load(std::memory_order_relaxed)) {
        return;
    }

    if(g_pos.x < 0.f || g_pos.y < 0.f) {
        const ImVec2 viewport = ImGui::GetIO().DisplaySize;
        g_pos = ImVec2 { (viewport.x - g_size.x) * 0.5f, (viewport.y - g_size.y) * 0.5f };
    }

    ImGui::GetIO().MouseDrawCursor = true;

    ImGui::SetNextWindowPos(g_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(g_size, ImGuiCond_Always);

    const ImRect title_bar = paint::window_begin("##tweaker_menu", k_title);

    const ImVec2 win_size = ImGui::GetWindowSize();
    const ImVec2 p_min = ImGui::GetWindowPos();
    const ImVec2 p_max { p_min.x + win_size.x, p_min.y + win_size.y };
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Drag-move: hit-test the title bar, own the whole gesture (window has NoMove - ImGui never
    // does this for us). Through kit::interact rather than raw rect geometry: a popup lying over the
    // title bar must take its own click, not start a window drag underneath it as well.
    const tw::ui::kit::interaction title = tw::ui::kit::interact(title_bar, ImGui::GetID("##title"), tw::ui::kit::placement::overlay);
    if(!g_dragging_move && !g_dragging_resize && title.left_down) {
        g_dragging_move = true;
        g_drag_start_mouse = ImGui::GetMousePos();
        g_drag_start_value = g_pos;
    }
    if(g_dragging_move) {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetMousePos();
            g_pos =
                ImVec2 { g_drag_start_value.x + (mouse.x - g_drag_start_mouse.x), g_drag_start_value.y + (mouse.y - g_drag_start_mouse.y) };
        }
        else {
            g_dragging_move = false;
            tw::ui::overlay_config::set_menu_pos(g_pos);
            tw::ui::overlay_config::request_save();
        }
    }

    // Drag-resize: bottom-right corner grip. The part of the corner the tab_view's content child covers
    // belongs to the child (its scrollbar ends there); the grip answers in the strip the child leaves free.
    paint::resize_grip(draw, p_max);
    const tw::ui::kit::interaction grip =
        tw::ui::kit::interact(paint::resize_grip_rect(p_max), ImGui::GetID("##resize"), tw::ui::kit::placement::overlay);
    if(!g_dragging_move && !g_dragging_resize && grip.left_down) {
        g_dragging_resize = true;
        g_drag_start_mouse = ImGui::GetMousePos();
        g_drag_start_value = g_size;
    }
    if(g_dragging_resize) {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const ImVec2 delta { mouse.x - g_drag_start_mouse.x, mouse.y - g_drag_start_mouse.y };
            g_size = ImVec2 {
                (std::max)(g_active_min_size.x, g_drag_start_value.x + delta.x),
                (std::max)(g_active_min_size.y, g_drag_start_value.y + delta.y),
            };
        }
        else {
            g_dragging_resize = false;
            tw::ui::overlay_config::set_menu_size(g_size);
            tw::ui::overlay_config::request_save();
        }
    }

    ImGui::SetCursorScreenPos(ImVec2 { p_min.x + k_padding, title_bar.Max.y + k_padding });
    const float content_w = win_size.x - k_padding * 2.f;
    const float content_h = win_size.y - k_title_h - k_padding * 2.f;

    const bool online = snapshot.host_connected;

    g_tabs.set_size(ImVec2 { content_w, content_h });
    g_tabs.begin();
    if(g_tabs.begin_view(0)) {
        if(online) {
            draw_skins_tab(snapshot);
        }
        else {
            draw_offline_stub();
        }
        g_tabs.end_view();
    }
    if(g_tabs.begin_view(1)) {
        if(online) {
            draw_tweaks_tab(snapshot);
        }
        else {
            draw_offline_stub();
        }
        g_tabs.end_view();
    }
    if(g_tabs.begin_view(k_player_tab)) {
        if(online) {
            tw::ui::plugins::interactive::player::draw(qp_snapshot);
        }
        else {
            draw_offline_stub();
        }
        g_tabs.end_view();
    }
    if(g_tabs.begin_view(3)) {
        draw_settings_tab();
        g_tabs.end_view();
    }
    for(std::size_t i = 0; i < g_extra_tabs.size(); ++i) {
        if(g_tabs.begin_view(k_first_extra_tab + static_cast<int>(i))) {
            g_extra_tabs[i].draw();
            g_tabs.end_view();
        }
    }
    g_tabs.end();

    ImGui::End();

    // Grown after the frame, not before it: the Player tab needs more room than the others, and
    // asking for the size it wants requires ImGui's font metrics, which are only valid inside a
    // frame. One-shot on entering the tab, and only ever upward - the user's saved size is theirs,
    // and silently shrinking the window back on leaving would fight every resize they make.
    // The stub fits any size the menu can have, so an offline Player tab asks for nothing.
    if(g_tabs.selected_tab() == k_player_tab && online) {
        const ImVec2 wanted = window_size_for(player::desired_content_size(qp_snapshot));
        g_active_min_size = ImVec2 { (std::max)(k_min_size.x, wanted.x), (std::max)(k_min_size.y, wanted.y) };

        // Grow only on the way in. Re-applying it every frame would undo any deliberate enlargement
        // the user made, and the clamp above already stops the window going the other way.
        if(g_last_tab != k_player_tab) {
            grow_to_fit(g_active_min_size);
        }
    }
    else {
        g_active_min_size = k_min_size;
    }

    g_last_tab = g_tabs.selected_tab();
}
} // namespace tw::ui::plugins::interactive::menu
