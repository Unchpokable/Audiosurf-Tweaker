#include "pch.hxx"

#include "skybox/sky_panel.hxx"

#include "skybox/sky_program.hxx"
#include "skybox/sky_ui.hxx"

#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/button.hxx"
#include "ui/widgets/color_field.hxx"
#include "ui/widgets/detail/draw.hxx"
#include "ui/widgets/slider.hxx"
#include "ui/widgets/tab_view.hxx"

#include <imgui.h>

namespace
{
namespace theme = tw::ui::theme;
namespace detail = tw::ui::widgets::detail;
namespace menu = tw::ui::plugins::interactive::menu;

using tw::ui::widgets::button;
using tw::ui::widgets::color_field;
using tw::ui::widgets::slider;
using tw::ui::widgets::tab_view;

// Chrome measurements taken from menu.cxx rather than invented - the panel has to read as the same
// window family, and two windows disagreeing by two pixels of title bar look like a bug.
constexpr float k_rounding = 5.f;
constexpr float k_title_h = 32.f;
constexpr float k_padding = 10.f;
constexpr float k_width = 320.f;
constexpr float k_dock_gap = 8.f;
constexpr float k_footer_h = 28.f;
constexpr float k_close_size = 22.f;

// Heading for parameters declared before any `group` line. They are drawn first, which is where the
// annotation puts them, and "General" is the only honest name for a bucket the author did not name.
constexpr const char* k_ungrouped = "General";

bool g_open = false;
bool g_focus_pending = false;

// One knob, addressed the way skybox::set_layer_param wants it.
//
// A sky is layers now, so an index into one parameter list is not an address any more. The widget
// index is separate from both because the widgets are one flat run over every layer, which is what
// keeps them addressable by a single number after the pages have shuffled them into groups.
struct row {
    int layer {};
    int index {};
    int widget {};
};

struct group_page {
    std::string label;
    std::vector<row> rows;
};

std::vector<group_page> g_pages;

// One widget per knob of every layer, in the order the layers were walked. Only the entry matching
// the parameter's kind is ever ticked; the other is dead weight worth a few hundred bytes and buys
// index arithmetic that cannot drift.
std::vector<slider> g_sliders;
std::vector<color_field> g_colors;

tab_view g_tabs { "sky_param_tabs" };
button g_reset_btn { "sky_param_reset", { 0.f, k_footer_h } };

// What the pages were built from. The pointer alone is not enough: a hot reload recompiles in place
// and can change the parameter set without changing the program - and a sky can now bring several
// layers, any one of which may have been the one that changed.
const tw::skybox::sky_program* g_built_program = nullptr;
std::size_t g_built_layers = 0;
unsigned int g_built_generation = 0;

// Cheap "has anything about this list of layers changed" - the sum of their parameter generations,
// which every rebuild of any of them bumps.
unsigned int layers_generation(std::span<tw::skybox::sky_program* const> layers) noexcept
{
    unsigned int sum = 0;
    for(const tw::skybox::sky_program* layer : layers) {
        sum += layer->params_generation;
    }

    return sum;
}

// Remembered by name, not by index. Switching shaders rebuilds the strip, and a shader whose second
// group is "Clouds" should not inherit the selection of one whose second group was "Stars".
std::string g_selected_label;

void rebuild(std::span<tw::skybox::sky_program* const> layers)
{
    g_pages.clear();
    g_sliders.clear();
    g_colors.clear();

    int widget = 0;

    for(std::size_t layer_index = 0; layer_index < layers.size(); ++layer_index) {
        const auto& params = layers[layer_index]->params;

        for(std::size_t i = 0; i < params.size(); ++i) {
            const tw::skybox::sky_param& param = params[i];

            // The key is unique across the whole sky, not just within one shader - a package layer
            // keys by "<layer>.<id>" and a shared knob by its path, precisely so that two layers
            // cannot hand ImGui the same id and produce two sliders that move together.
            g_sliders.emplace_back(param.key.c_str());
            g_colors.emplace_back(param.key.c_str());

            g_sliders.back().set_label(param.label);
            g_colors.back().set_label(param.label);

            if(!param.is_color()) {
                g_sliders.back().set_range(param.min_value, param.max_value);
                g_sliders.back().set_value(param.value[0]);
            }
            else {
                g_colors.back().set_color(ImVec4 { param.value[0], param.value[1], param.value[2], 1.f });
            }

            // order_by_group() has already brought each group together within a layer, so a page
            // break is a change of group name. Across layers the same name merges into one page,
            // which is what an author asking for it means: the group is theirs to name.
            const std::string_view group = param.group.empty() ? std::string_view { k_ungrouped } : std::string_view { param.group };

            group_page* page = nullptr;
            for(group_page& candidate : g_pages) {
                if(candidate.label == group) {
                    page = &candidate;
                    break;
                }
            }

            if(page == nullptr) {
                group_page created;
                created.label.assign(group);
                g_pages.push_back(std::move(created));
                page = &g_pages.back();
            }

            page->rows.push_back(row { static_cast<int>(layer_index), static_cast<int>(i), widget });
            ++widget;
        }
    }

    g_built_program = layers.empty() ? nullptr : layers.front();
    g_built_layers = layers.size();
    g_built_generation = layers_generation(layers);

    if(g_pages.size() < 2) {
        return;
    }

    std::vector<std::string_view> labels;
    labels.reserve(g_pages.size());
    for(const group_page& page : g_pages) {
        labels.emplace_back(page.label);
    }

    // set_tabs is heavy and must run outside begin()/end() - both satisfied here, this only ever
    // runs on a program or recompile change.
    g_tabs.set_tabs(labels);
    g_tabs.set_rounding(k_rounding);

    for(std::size_t i = 0; i < g_pages.size(); ++i) {
        if(g_pages[i].label == g_selected_label) {
            g_tabs.set_selected_tab(static_cast<int>(i));
            return;
        }
    }

    // No group of that name here. set_tabs keeps an in-range index, which would leave the strip
    // pointing at whatever happens to sit at the old position - so say where to land explicitly.
    g_tabs.set_selected_tab(0);
    g_selected_label = g_pages.front().label;
}

void draw_param_rows(std::span<tw::skybox::sky_program* const> layers, const std::vector<row>& rows)
{
    for(const row& entry : rows) {
        if(static_cast<std::size_t>(entry.layer) >= layers.size()) {
            continue;
        }

        const tw::skybox::sky_program& program = *layers[static_cast<std::size_t>(entry.layer)];
        if(static_cast<std::size_t>(entry.index) >= program.params.size()) {
            continue;
        }

        const tw::skybox::sky_param& param = program.params[static_cast<std::size_t>(entry.index)];

        // Components the control does not own are carried through untouched: a scalar parameter
        // names one channel of a register, and the rest of that register is somebody else's knob.
        std::array<float, 3> next = param.value;
        bool moved = false;
        bool commit = false;

        if(param.is_color()) {
            color_field& widget = g_colors[static_cast<std::size_t>(entry.widget)];
            widget.set_color(ImVec4 { param.value[0], param.value[1], param.value[2], 1.f });
            widget.update();

            const ImVec4 picked = widget.color();
            next = { picked.x, picked.y, picked.z };
            moved = widget.changed();
            commit = widget.committed();
        }
        else {
            slider& widget = g_sliders[static_cast<std::size_t>(entry.widget)];
            widget.set_range(param.min_value, param.max_value);
            widget.set_value(param.value[0]);
            widget.update();

            next[0] = widget.value();
            moved = widget.changed();
            commit = widget.committed();
        }

        // One call even when a typed value both moves and commits in the same frame. `persist` is
        // what separates the live preview from the settings write - see skybox::set_program_param.
        if(moved || commit) {
            tw::skybox::set_layer_param(entry.layer, entry.index, next, commit);
        }
    }
}

// Bordered scroll container, the same recipe list_view and the Player tab use for their panels.
void begin_plain_body(float height)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::surface_muted);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, k_rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { 8.f, 8.f });
    ImGui::BeginChild("##sky_param_body", ImVec2 { 0.f, height }, ImGuiChildFlags_Borders);
}

void end_plain_body()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// Two strokes rather than an SVG: there is no close.svg in assets/icons, and the menu's own resize
// grip is hand-drawn for the same reason - one shape used once does not need to become an asset.
bool draw_close_button(ImDrawList* draw, ImVec2 title_max, float title_y)
{
    const ImVec2 slot { title_max.x - k_padding - k_close_size, title_y + (k_title_h - k_close_size) * 0.5f };

    ImGui::SetCursorScreenPos(slot);
    ImGui::InvisibleButton("##close", ImVec2 { k_close_size, k_close_size });

    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    const ImVec2 center { slot.x + k_close_size * 0.5f, slot.y + k_close_size * 0.5f };
    if(hovered) {
        draw->AddCircleFilled(center, k_close_size * 0.5f, detail::to_u32(theme::surface_hover));
    }

    const ImU32 col = detail::to_u32(hovered ? theme::text_primary : theme::text_muted);
    constexpr float k_arm = 4.5f;
    draw->AddLine(ImVec2 { center.x - k_arm, center.y - k_arm }, ImVec2 { center.x + k_arm, center.y + k_arm }, col, 1.6f);
    draw->AddLine(ImVec2 { center.x - k_arm, center.y + k_arm }, ImVec2 { center.x + k_arm, center.y - k_arm }, col, 1.6f);

    return clicked;
}

// Right of the menu by preference, left when that would run off the screen. Never floating on its
// own: the panel is a wing of the menu, and a second draggable window is a second thing to arrange.
ImVec2 dock_position(ImVec2 menu_pos, ImVec2 menu_size, ImVec2 size, ImVec2 viewport)
{
    float x = menu_pos.x + menu_size.x + k_dock_gap;
    if(x + size.x > viewport.x) {
        // Neither side fits: sit flush against the right edge and accept overlapping the menu, which
        // is what SetNextWindowFocus on open is there for.
        const float left = menu_pos.x - size.x - k_dock_gap;
        x = left >= 0.f ? left : (std::max)(0.f, viewport.x - size.x);
    }

    const float y = std::clamp(menu_pos.y, 0.f, (std::max)(0.f, viewport.y - size.y));
    return ImVec2 { x, y };
}
} // namespace

namespace tw::skybox::panel
{
void open() noexcept
{
    if(!g_open) {
        g_focus_pending = true;
    }
    g_open = true;
}

void close() noexcept
{
    g_open = false;
}

void toggle() noexcept
{
    if(g_open) {
        close();
    }
    else {
        open();
    }
}

bool open_requested() noexcept
{
    return g_open;
}

void draw(const status& status) noexcept
{
    if(!g_open) {
        return;
    }

    // Every one of these hides the window without touching g_open. A cube map has no knobs, another
    // tab is not this tab - but neither is a decision to stop tuning, and coming back must not cost
    // a second click on Parameters.
    if(!menu::extra_tab_selected(::tw::skybox::ui::tab_handle())) {
        return;
    }

    ImVec2 menu_pos {};
    ImVec2 menu_size {};
    if(!menu::window_rect(menu_pos, menu_size)) {
        return;
    }

    const std::span<sky_program* const> layers = active_layers();
    if(layers.empty() || status.program == nullptr) {
        return;
    }

    if(layers.front() != g_built_program || layers.size() != g_built_layers || layers_generation(layers) != g_built_generation) {
        rebuild(layers);
    }

    if(g_pages.empty()) {
        return;
    }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const ImVec2 size { k_width, menu_size.y };
    const ImVec2 pos = dock_position(menu_pos, menu_size, size, viewport);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);

    if(g_focus_pending) {
        // Only matters where the panel had to overlap the menu; harmless where it did not.
        ImGui::SetNextWindowFocus();
        g_focus_pending = false;
    }

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove
                                       | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
                                       | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    // Border colour zeroed as well as its size, and NoNav set: see the long note in menu.cxx - a
    // NoBackground window still draws a square border and a nav highlight, both of which peek out
    // from under the rounded corners this paints itself.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, k_rounding);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4 { 0.f, 0.f, 0.f, 0.f });
    ImGui::Begin("##tweaker_sky_params", nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    const ImVec2 p_min = ImGui::GetWindowPos();
    const ImVec2 win_size = ImGui::GetWindowSize();
    const ImVec2 p_max { p_min.x + win_size.x, p_min.y + win_size.y };

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(p_min, p_max, detail::to_u32(theme::surface), k_rounding);
    draw->AddRect(p_min, p_max, detail::to_u32(theme::border_window), k_rounding);

    const ImVec2 title_max { p_max.x, p_min.y + k_title_h };
    draw->AddRectFilled(p_min, title_max, detail::to_u32(theme::surface_elevated), k_rounding, ImDrawFlags_RoundCornersTop);

    const float title_text_y = p_min.y + (k_title_h - ImGui::GetFontSize()) * 0.5f;
    detail::add_text_ellipsis(draw,
        ImVec2 { p_min.x + 12.f, title_text_y },
        title_max.x - k_padding - k_close_size - 8.f,
        detail::to_u32(theme::text_primary),
        layers.front()->display_name.c_str());

    if(draw_close_button(draw, title_max, p_min.y)) {
        close();
    }

    const float content_w = win_size.x - k_padding * 2.f;
    const float content_h = win_size.y - k_title_h - k_padding * 2.f;
    const float body_h = (std::max)(60.f, content_h - k_footer_h - ImGui::GetStyle().ItemSpacing.y);

    ImGui::SetCursorScreenPos(ImVec2 { p_min.x + k_padding, title_max.y + k_padding });

    if(g_pages.size() > 1) {
        g_tabs.set_size(ImVec2 { content_w, body_h });
        g_tabs.begin();
        for(std::size_t i = 0; i < g_pages.size(); ++i) {
            if(g_tabs.begin_view(static_cast<int>(i))) {
                draw_param_rows(layers, g_pages[i].rows);
                g_tabs.end_view();
            }
        }
        g_tabs.end();

        if(g_tabs.selection_changed()) {
            const int selected = g_tabs.selected_tab();
            if(selected >= 0 && static_cast<std::size_t>(selected) < g_pages.size()) {
                g_selected_label = g_pages[static_cast<std::size_t>(selected)].label;
            }
        }
    }
    else if(!g_pages.empty()) {
        // A single-tab strip reads as a broken control. This is the ordinary case, not an edge one:
        // sky_gradient and sky_probe declare no annotation of their own and inherit only the shared
        // palette, so they have exactly one group.
        begin_plain_body(body_h);
        draw_param_rows(layers, g_pages.front().rows);
        end_plain_body();
    }

    // Placed absolutely rather than after the body: tab_view and the plain child claim their space
    // differently, and the footer should sit on the same line either way.
    ImGui::SetCursorScreenPos(ImVec2 { p_min.x + k_padding, p_max.y - k_padding - k_footer_h });
    g_reset_btn.set_size(ImVec2 { content_w, k_footer_h });
    g_reset_btn.update("Reset to defaults");
    if(g_reset_btn.clicked()) {
        reset_sky_params();
        tw::ui::plugins::statics::notefeed::push("Sky parameters reset");
    }

    ImGui::End();
}
} // namespace tw::skybox::panel
