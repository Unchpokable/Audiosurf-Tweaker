// No TweakerPlugin PCH here - shared with smoke_test, same convention as texture_cache.cxx.
// No windows.h either: the Insert hotkey is no longer polled from here, it arrives as a WndProc
// message from whichever harness owns the message pump (see menu.hxx, toggle_visible()).
#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <algorithm>
#include <atomic>
#include <span>
#include <string>
#include <vector>

#include "ui/plugins/interactive/menu.hxx"

#include "ui/overlay_config.hxx"

#include "ui/pending_actions.hxx"

#include "ui/theme.hxx"

#include "ui/widgets/button.hxx"
#include "ui/widgets/color_picker.hxx"
#include "ui/widgets/detail/draw.hxx"
#include "ui/widgets/item_group.hxx"
#include "ui/widgets/list_view.hxx"
#include "ui/widgets/popup_menu.hxx"
#include "ui/widgets/tab_view.hxx"
#include "ui/widgets/toggle.hxx"

namespace
{
using tw::ui::widgets::button;
using tw::ui::widgets::color_picker;
using tw::ui::widgets::item_group;
using tw::ui::widgets::list_item_content;
using tw::ui::widgets::list_view;
using tw::ui::widgets::popup_menu;
using tw::ui::widgets::tab_view;
using tw::ui::widgets::toggle;

namespace theme = tw::ui::theme;

// Live theme color editor (Settings tab) - one row per tw::ui::theme field, grouped into
// item_group boxes matching the section comments in theme.hxx.
struct theme_color_entry {
    const char* key;   // theme.hxx field name -> stable widget id
    const char* label; // display text next to the swatch
    ImVec4* color;     // direct pointer into tw::ui::theme - edits apply live
};

struct theme_color_row {
    const theme_color_entry* entry;
    popup_menu popup;
    color_picker picker;

    // Hover-glow animation state (button.cxx-style tween: retarget on hover-state change, step
    // via detail::dt_ms() every frame) - purely a discoverability affordance for the RMB popup.
    bool was_hovered = false;
    float hover_t = 0.f;
    tweeny::tween<float> hover_tween {};

    explicit theme_color_row(const theme_color_entry& e)
        : entry(&e), popup((std::string("menu_theme_popup_") + e.key).c_str(), e.label),
          picker((std::string("menu_theme_picker_") + e.key).c_str(), ImVec2 { 260.f, 0.f })
    {
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

constexpr float k_rounding = 5.f;
constexpr float k_title_h = 32.f;
constexpr float k_resize_grip = 16.f;
constexpr float k_padding = 10.f;
constexpr float k_theme_group_gap = 12.f; // horizontal gap between theme item_groups sharing a row
constexpr int k_swatch_hover_ms = 150;    // matches button.cxx's k_hover_ms
constexpr ImVec2 k_min_size { 320.f, 260.f };
constexpr const char* k_title = "Audiosurf Tweaker";

// Written from the message-pump thread (toggle_visible/set_visible), read from the render thread
// (update) and from the dinput hooks on whichever thread the game polls its devices.
std::atomic<bool> g_visible = false;

tab_view g_tabs { "menu_tabs" };
list_view g_skins_list { "menu_skins" };
button g_skins_apply_btn { "menu_skins_apply", { 100.f, 28.f } };
std::vector<toggle> g_tweak_toggles;
bool g_widgets_ready = false;

ImVec2 g_pos { -1.f, -1.f }; // sentinel: pick a centered default on first show
ImVec2 g_size { 420.f, 520.f };

bool g_dragging_move = false;
bool g_dragging_resize = false;
ImVec2 g_drag_start_mouse {};
ImVec2 g_drag_start_value {};

std::vector<std::string> g_last_skin_names;

std::vector<theme_group> g_theme_groups;

button g_feed_left { "menu_feed_left", { 90.f, 28.f } };
button g_feed_right { "menu_feed_right", { 90.f, 28.f } };
button g_pins_left { "menu_pins_left", { 90.f, 28.f } };
button g_pins_right { "menu_pins_right", { 90.f, 28.f } };
button g_theme_reset_btn { "menu_theme_reset", { 100.f, 28.f } };

void ensure_widgets_ready()
{
    if(g_widgets_ready) {
        return;
    }

    static const std::string_view labels[] = { "Skins", "Tweaks", "Settings" };
    g_tabs.set_tabs(labels);
    g_tabs.set_rounding(k_rounding);

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

void draw_skins_tab(const tw::ui::overlay_state::cache& snapshot)
{
    if(snapshot.skin_names != g_last_skin_names) {
        std::vector<list_item_content> items;
        items.reserve(snapshot.skin_names.size());
        for(const auto& name : snapshot.skin_names) {
            items.push_back(list_item_content { .text = name });
        }
        g_skins_list.set_items(items);
        g_last_skin_names = snapshot.skin_names;
    }

    const std::string_view display_skin = tw::ui::pending_actions::skin_display_name(snapshot);
    ImGui::TextUnformatted(display_skin.empty() ? "Current skin: (none)" : ("Current skin: " + std::string(display_skin)).c_str());
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
    const std::string selected_name = has_selection ? g_last_skin_names[static_cast<std::size_t>(selected)] : std::string {};
    const bool already_current = has_selection && selected_name == display_skin;
    const bool pending = tw::ui::pending_actions::has_pending_skin();

    g_skins_apply_btn.update(pending ? "Applying..." : "Apply");
    if(g_skins_apply_btn.clicked() && has_selection && !already_current && !pending) {
        tw::ui::pending_actions::request_skin(selected_name);
    }
}

void draw_tweaks_tab(const tw::ui::overlay_state::cache& snapshot)
{
    const auto& ids = tw::ui::overlay_state::all_tweak_ids();
    for(std::size_t i = 0; i < ids.size(); ++i) {
        const bool enabled = tw::ui::pending_actions::tweak_display_enabled(snapshot, ids[i]);
        g_tweak_toggles[i].set_checked(enabled); // no-op if unchanged - see toggle::set_checked

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tw::ui::overlay_state::tweak_display_name(ids[i]).data());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 44.f);
        g_tweak_toggles[i].update();
        if(g_tweak_toggles[i].changed()) {
            // Cheap idempotent write (unlike a skin install) - fires straight from the click, no
            // Apply gate needed. See ui/pending_actions.hxx for the confirm/timeout/rollback logic.
            tw::ui::pending_actions::request_tweak(ids[i], g_tweak_toggles[i].checked());
        }
    }
}

void draw_side_row(const char* label,
    tw::ui::overlay_config::side current,
    button& left_btn,
    button& right_btn,
    void (*setter)(tw::ui::overlay_config::side))
{
    using tw::ui::overlay_config::side;

    ImGui::TextUnformatted(label);
    left_btn.set_label(current == side::left ? "Left [*]" : "Left");
    left_btn.update();
    if(left_btn.clicked()) {
        setter(side::left);
        tw::ui::overlay_config::request_save();
    }
    ImGui::SameLine();
    right_btn.set_label(current == side::right ? "Right [*]" : "Right");
    right_btn.update();
    if(right_btn.clicked()) {
        setter(side::right);
        tw::ui::overlay_config::request_save();
    }
}

// Serializes the live (possibly edited) theme and writes it to disk - called after every color
// change and after a Default reset, mirroring how draw_side_row saves right after each setter.
void persist_theme()
{
    tw::ui::overlay_config::set_theme_overrides(tw::ui::theme::to_config());
    tw::ui::overlay_config::request_save();
}

// One row: outlined circle swatch (live-reflects *entry->color) + label; RMB opens a popup_menu
// with a color_picker bound directly to the same theme field, so edits apply/persist instantly.
void draw_theme_color_row(theme_color_row& row)
{
    ImGui::PushID(row.entry->key);

    const float d = ImGui::GetFrameHeight();
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##swatch", ImVec2 { d, d });

    // Hover-glow affordance (RMB-for-picker isn't otherwise discoverable) - same tween pattern as
    // button::retarget_hover/update, just driving a glow instead of a fill color.
    const bool hovered = ImGui::IsItemHovered();
    if(hovered != row.was_hovered) {
        row.hover_tween = tweeny::from(row.hover_t).to(hovered ? 1.f : 0.f).during(k_swatch_hover_ms).via(tweeny::easing::cubicOut);
        row.was_hovered = hovered;
    }
    if(!row.hover_tween.isFinished()) {
        row.hover_t = row.hover_tween.step(tw::ui::widgets::detail::dt_ms());
    }

    const ImVec2 center { cursor.x + d * 0.5f, cursor.y + d * 0.5f };
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Glow tint forces full alpha - several theme colors are intentionally low/zero-alpha overlay
    // tints (e.g. accent_ghost, accent_soft) and would otherwise render an invisible glow.
    const ImVec4 glow_tint { row.entry->color->x, row.entry->color->y, row.entry->color->z, 1.f };
    tw::ui::widgets::detail::add_circle_glow(draw, center, d * 0.5f, glow_tint, row.hover_t);
    draw->AddCircleFilled(center, d * 0.5f - 1.5f, tw::ui::widgets::detail::to_u32(*row.entry->color));
    draw->AddCircle(center, d * 0.5f, tw::ui::widgets::detail::to_u32(theme::border));

    if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        row.popup.open();
    }

    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(row.entry->label);

    if(row.popup.opened()) {
        row.popup.begin();
        row.picker.update(*row.entry->color); // bidirectional bind straight into theme::*
        if(row.picker.changed()) {
            persist_theme();
        }
        row.popup.end();
    }

    ImGui::PopID();
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

void draw_settings_tab()
{
    draw_side_row("Notefeed / watermark corner",
        tw::ui::overlay_config::feed_side(),
        g_feed_left,
        g_feed_right,
        &tw::ui::overlay_config::set_feed_side);
    ImGui::Spacing();
    draw_side_row("Pins side", tw::ui::overlay_config::pins_side(), g_pins_left, g_pins_right, &tw::ui::overlay_config::set_pins_side);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextUnformatted("Color theme");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 100.f);
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
namespace detail = tw::ui::widgets::detail;

void initialize() noexcept
{
    g_widgets_ready = false;
    g_visible.store(false, std::memory_order_relaxed);
    g_last_skin_names.clear();
}

void shutdown() noexcept
{
    g_visible.store(false, std::memory_order_relaxed);
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

void update(const tw::ui::overlay_state::cache& snapshot) noexcept
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

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove
                                       | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
                                       | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    // NoBackground only skips ImGui's own ImGuiCol_WindowBg fill - it still draws its own
    // straight-cornered ImGuiCol_Border outline (WindowBorderSize=0 below) and, since the window
    // can take nav focus, a keyboard-nav highlight rect (NoNav above) - both independent of
    // NoBackground and both peeked out from under our rounded corners as a stray gray edge. Belt
    // and suspenders: zero the border color too, not just its size. Our own AddRect border below
    // is the only outline that should ever show.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, k_rounding);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4 { 0.f, 0.f, 0.f, 0.f });
    ImGui::Begin("##tweaker_menu", nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    const ImVec2 win_pos = ImGui::GetWindowPos();
    const ImVec2 win_size = ImGui::GetWindowSize();
    const ImVec2 p_min = win_pos;
    const ImVec2 p_max { win_pos.x + win_size.x, win_pos.y + win_size.y };

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p_min, p_max, detail::to_u32(theme::surface), k_rounding);
    draw->AddRect(p_min, p_max, detail::to_u32(theme::border_window), k_rounding);

    const ImVec2 title_min = p_min;
    const ImVec2 title_max { p_max.x, p_min.y + k_title_h };
    draw->AddRectFilled(title_min, title_max, detail::to_u32(theme::surface_elevated), k_rounding, ImDrawFlags_RoundCornersTop);

    const ImVec2 title_text_size = ImGui::CalcTextSize(k_title);
    draw->AddText(
        ImVec2 { title_min.x + 12.f, title_min.y + (k_title_h - title_text_size.y) * 0.5f }, detail::to_u32(theme::text_primary), k_title);

    // Drag-move: hit-test the title bar, own the whole gesture (window has NoMove - ImGui never
    // does this for us).
    const bool mouse_in_title = ImGui::IsMouseHoveringRect(title_min, title_max);
    if(!g_dragging_move && !g_dragging_resize && mouse_in_title && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
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

    // Drag-resize: bottom-right corner grip. The hit region covers the full corner (easy to grab),
    // but the visual affordance is a few inset diagonal dashes rather than a solid triangle
    // touching the true corner point - a filled triangle anchored at p_max pokes out past the
    // rounded background's silhouette there (the background's corner curve cuts inward before
    // reaching p_max), reading as a stray straight-edged nub.
    const ImVec2 grip_min { p_max.x - k_resize_grip, p_max.y - k_resize_grip };
    const ImVec2 grip_anchor { p_max.x - k_rounding - 2.f, p_max.y - k_rounding - 2.f };
    const ImU32 grip_col = detail::to_u32(theme::border_strong);
    for(int i = 0; i < 3; ++i) {
        const float d = static_cast<float>(i) * 4.5f + 3.f;
        draw->AddLine(ImVec2 { grip_anchor.x - d, grip_anchor.y + 2.f }, ImVec2 { grip_anchor.x + 2.f, grip_anchor.y - d }, grip_col, 1.5f);
    }

    const bool mouse_in_grip = ImGui::IsMouseHoveringRect(grip_min, p_max);
    if(!g_dragging_move && !g_dragging_resize && mouse_in_grip && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        g_dragging_resize = true;
        g_drag_start_mouse = ImGui::GetMousePos();
        g_drag_start_value = g_size;
    }
    if(g_dragging_resize) {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const ImVec2 delta { mouse.x - g_drag_start_mouse.x, mouse.y - g_drag_start_mouse.y };
            g_size = ImVec2 {
                (std::max)(k_min_size.x, g_drag_start_value.x + delta.x),
                (std::max)(k_min_size.y, g_drag_start_value.y + delta.y),
            };
        }
        else {
            g_dragging_resize = false;
            tw::ui::overlay_config::set_menu_size(g_size);
            tw::ui::overlay_config::request_save();
        }
    }

    ImGui::SetCursorScreenPos(ImVec2 { p_min.x + k_padding, title_max.y + k_padding });
    const float content_w = win_size.x - k_padding * 2.f;
    const float content_h = win_size.y - k_title_h - k_padding * 2.f;

    g_tabs.set_size(ImVec2 { content_w, content_h });
    g_tabs.begin();
    if(g_tabs.begin_view(0)) {
        draw_skins_tab(snapshot);
        g_tabs.end_view();
    }
    if(g_tabs.begin_view(1)) {
        draw_tweaks_tab(snapshot);
        g_tabs.end_view();
    }
    if(g_tabs.begin_view(2)) {
        draw_settings_tab();
        g_tabs.end_view();
    }
    g_tabs.end();

    ImGui::End();
}
} // namespace tw::ui::plugins::interactive::menu
