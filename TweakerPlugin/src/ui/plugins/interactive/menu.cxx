#include "ui/plugins/interactive/menu.hxx"

#include "ui/overlay_config.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/detail/draw.hxx"
#include "ui/widgets/list_view.hxx"
#include "ui/widgets/tab_view.hxx"
#include "ui/widgets/toggle.hxx"

// No TweakerPlugin PCH here - shared with smoke_test, same convention as texture_cache.cxx.
// GetAsyncKeyState/VK_INSERT works identically in both the injected DLL and the standalone smoke
// Win32 window.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace tw::ui::plugins::interactive::menu
{
namespace detail = tw::ui::widgets::detail;

namespace
{
using tw::ui::widgets::button;
using tw::ui::widgets::list_item_content;
using tw::ui::widgets::list_view;
using tw::ui::widgets::tab_view;
using tw::ui::widgets::toggle;

constexpr float k_rounding = 5.f;
constexpr float k_title_h = 32.f;
constexpr float k_resize_grip = 16.f;
constexpr float k_padding = 10.f;
constexpr ImVec2 k_min_size {320.f, 260.f};
constexpr const char* k_title = "Audiosurf Tweaker";

bool g_visible = false;
bool g_insert_was_down = false;

tab_view g_tabs {"menu_tabs"};
list_view g_skins_list {"menu_skins"};
std::vector<toggle> g_tweak_toggles;
bool g_widgets_ready = false;

ImVec2 g_pos {-1.f, -1.f}; // sentinel: pick a centered default on first show
ImVec2 g_size {420.f, 520.f};

bool g_dragging_move = false;
bool g_dragging_resize = false;
ImVec2 g_drag_start_mouse {};
ImVec2 g_drag_start_value {};

std::vector<std::string> g_last_skin_names;

char g_theme_buf[2048] = {};
bool g_theme_seeded = false;

button g_feed_left {"menu_feed_left", {90.f, 28.f}};
button g_feed_right {"menu_feed_right", {90.f, 28.f}};
button g_pins_left {"menu_pins_left", {90.f, 28.f}};
button g_pins_right {"menu_pins_right", {90.f, 28.f}};
button g_theme_apply {"menu_theme_apply", {120.f, 32.f}};

void ensure_widgets_ready()
{
    if(g_widgets_ready) {
        return;
    }

    static const std::string_view labels[] = {"Skins", "Tweaks", "Settings"};
    g_tabs.set_tabs(labels);
    g_tabs.set_rounding(k_rounding);

    g_tweak_toggles.clear();
    g_tweak_toggles.reserve(tw::ui::overlay_state::k_tweak_count);
    for(std::size_t i = 0; i < tw::ui::overlay_state::k_tweak_count; ++i) {
        const std::string id = "menu_tweak_" + std::to_string(i);
        g_tweak_toggles.emplace_back(id.c_str());
    }

    g_pos = tw::ui::overlay_config::menu_pos();
    g_size = tw::ui::overlay_config::menu_size();

    g_widgets_ready = true;
}

void poll_toggle_key() noexcept
{
    const bool down = (::GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if(down && !g_insert_was_down) {
        g_visible = !g_visible;
    }
    g_insert_was_down = down;
}

void draw_skins_tab(const tw::ui::overlay_state::cache& snapshot)
{
    if(snapshot.skin_names != g_last_skin_names) {
        std::vector<list_item_content> items;
        items.reserve(snapshot.skin_names.size());
        for(const auto& name : snapshot.skin_names) {
            items.push_back(list_item_content {.text = name});
        }
        g_skins_list.set_items(items);
        g_last_skin_names = snapshot.skin_names;
    }

    ImGui::TextUnformatted(
        snapshot.current_skin_name.empty() ? "Current skin: (none)" : ("Current skin: " + snapshot.current_skin_name).c_str());
    ImGui::Spacing();

    // Fully interactive (click moves the local highlight), but doesn't round-trip to the host -
    // reverse-sync (overlay -> host) isn't implemented yet, see overlay-protocol.md. The next
    // TW_OVL update from the host is the only thing that changes current_skin_name above.
    const float list_h = (std::max)(80.f, ImGui::GetContentRegionAvail().y);
    g_skins_list.set_size(ImVec2 {0.f, list_h});
    g_skins_list.update();
}

void draw_tweaks_tab(const tw::ui::overlay_state::cache& snapshot)
{
    const auto& ids = tw::ui::overlay_state::all_tweak_ids();
    for(std::size_t i = 0; i < ids.size(); ++i) {
        const bool enabled = tw::ui::overlay_state::is_tweak_enabled(snapshot, ids[i]);
        g_tweak_toggles[i].set_checked(enabled); // no-op if unchanged - see toggle::set_checked

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tw::ui::overlay_state::tweak_display_name(ids[i]).data());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 44.f);
        g_tweak_toggles[i].update();
        // clicked() -> handled internally, no host round-trip yet (see draw_skins_tab comment);
        // set_checked() above resnaps display to host truth on the next TW_OVL update either way.
    }
}

void draw_side_row(const char* label, tw::ui::overlay_config::side current, button& left_btn, button& right_btn, void (*setter)(tw::ui::overlay_config::side))
{
    using tw::ui::overlay_config::side;

    ImGui::TextUnformatted(label);
    left_btn.set_label(current == side::left ? "Left [*]" : "Left");
    left_btn.update();
    if(left_btn.clicked()) {
        setter(side::left);
        tw::ui::overlay_config::save();
    }
    ImGui::SameLine();
    right_btn.set_label(current == side::right ? "Right [*]" : "Right");
    right_btn.update();
    if(right_btn.clicked()) {
        setter(side::right);
        tw::ui::overlay_config::save();
    }
}

void draw_settings_tab()
{
    draw_side_row("Notefeed / watermark corner", tw::ui::overlay_config::feed_side(), g_feed_left, g_feed_right, &tw::ui::overlay_config::set_feed_side);
    ImGui::Spacing();
    draw_side_row("Pins side", tw::ui::overlay_config::pins_side(), g_pins_left, g_pins_right, &tw::ui::overlay_config::set_pins_side);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if(!g_theme_seeded) {
        const std::string& saved = tw::ui::overlay_config::theme_overrides();
        const std::size_t n = (std::min)(saved.size(), sizeof(g_theme_buf) - 1);
        std::copy_n(saved.data(), n, g_theme_buf);
        g_theme_buf[n] = '\0';
        g_theme_seeded = true;
    }

    ImGui::TextUnformatted("Theme overrides (key=#RRGGBB per line, groundwork for a future theme editor)");
    const float edit_h = (std::max)(80.f, ImGui::GetContentRegionAvail().y - 44.f);
    ImGui::InputTextMultiline("##theme_overrides", g_theme_buf, sizeof(g_theme_buf), ImVec2 {0.f, edit_h});

    g_theme_apply.update("Apply");
    if(g_theme_apply.clicked()) {
        tw::ui::overlay_config::set_theme_overrides(std::string(g_theme_buf));
        tw::ui::theme::apply_dark();
        tw::ui::theme::from_config(g_theme_buf);
        tw::ui::overlay_config::save();
    }
}
} // namespace

void initialize() noexcept
{
    g_widgets_ready = false;
    g_visible = false;
    g_insert_was_down = false;
    g_theme_seeded = false;
    g_last_skin_names.clear();
}

void shutdown() noexcept
{
    g_visible = false;
}

void update(const tw::ui::overlay_state::cache& snapshot) noexcept
{
    ensure_widgets_ready();
    poll_toggle_key();

    if(!g_visible) {
        return;
    }

    if(g_pos.x < 0.f || g_pos.y < 0.f) {
        const ImVec2 viewport = ImGui::GetIO().DisplaySize;
        g_pos = ImVec2 {(viewport.x - g_size.x) * 0.5f, (viewport.y - g_size.y) * 0.5f};
    }

    ImGui::SetNextWindowPos(g_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(g_size, ImGuiCond_Always);

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav;

    // NoBackground only skips ImGui's own ImGuiCol_WindowBg fill - it still draws its own
    // straight-cornered ImGuiCol_Border outline (WindowBorderSize=0 below) and, since the window
    // can take nav focus, a keyboard-nav highlight rect (NoNav above) - both independent of
    // NoBackground and both peeked out from under our rounded corners as a stray gray edge. Belt
    // and suspenders: zero the border color too, not just its size. Our own AddRect border below
    // is the only outline that should ever show.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, k_rounding);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4 {0.f, 0.f, 0.f, 0.f});
    ImGui::Begin("##tweaker_menu", nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    const ImVec2 win_pos = ImGui::GetWindowPos();
    const ImVec2 win_size = ImGui::GetWindowSize();
    const ImVec2 p_min = win_pos;
    const ImVec2 p_max {win_pos.x + win_size.x, win_pos.y + win_size.y};

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p_min, p_max, detail::to_u32(theme::surface), k_rounding);
    draw->AddRect(p_min, p_max, detail::to_u32(theme::border_window), k_rounding);

    const ImVec2 title_min = p_min;
    const ImVec2 title_max {p_max.x, p_min.y + k_title_h};
    draw->AddRectFilled(title_min, title_max, detail::to_u32(theme::surface_elevated), k_rounding, ImDrawFlags_RoundCornersTop);

    const ImVec2 title_text_size = ImGui::CalcTextSize(k_title);
    draw->AddText(
        ImVec2 {title_min.x + 12.f, title_min.y + (k_title_h - title_text_size.y) * 0.5f}, detail::to_u32(theme::text_primary), k_title);

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
            g_pos = ImVec2 {g_drag_start_value.x + (mouse.x - g_drag_start_mouse.x), g_drag_start_value.y + (mouse.y - g_drag_start_mouse.y)};
        }
        else {
            g_dragging_move = false;
            tw::ui::overlay_config::set_menu_pos(g_pos);
            tw::ui::overlay_config::save();
        }
    }

    // Drag-resize: bottom-right corner grip. The hit region covers the full corner (easy to grab),
    // but the visual affordance is a few inset diagonal dashes rather than a solid triangle
    // touching the true corner point - a filled triangle anchored at p_max pokes out past the
    // rounded background's silhouette there (the background's corner curve cuts inward before
    // reaching p_max), reading as a stray straight-edged nub.
    const ImVec2 grip_min {p_max.x - k_resize_grip, p_max.y - k_resize_grip};
    const ImVec2 grip_anchor {p_max.x - k_rounding - 2.f, p_max.y - k_rounding - 2.f};
    const ImU32 grip_col = detail::to_u32(theme::border_strong);
    for(int i = 0; i < 3; ++i) {
        const float d = static_cast<float>(i) * 4.5f + 3.f;
        draw->AddLine(
            ImVec2 {grip_anchor.x - d, grip_anchor.y + 2.f}, ImVec2 {grip_anchor.x + 2.f, grip_anchor.y - d}, grip_col, 1.5f);
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
            const ImVec2 delta {mouse.x - g_drag_start_mouse.x, mouse.y - g_drag_start_mouse.y};
            g_size = ImVec2 {
                (std::max)(k_min_size.x, g_drag_start_value.x + delta.x),
                (std::max)(k_min_size.y, g_drag_start_value.y + delta.y),
            };
        }
        else {
            g_dragging_resize = false;
            tw::ui::overlay_config::set_menu_size(g_size);
            tw::ui::overlay_config::save();
        }
    }

    ImGui::SetCursorScreenPos(ImVec2 {p_min.x + k_padding, title_max.y + k_padding});
    const float content_w = win_size.x - k_padding * 2.f;
    const float content_h = win_size.y - k_title_h - k_padding * 2.f;

    g_tabs.set_size(ImVec2 {content_w, content_h});
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
