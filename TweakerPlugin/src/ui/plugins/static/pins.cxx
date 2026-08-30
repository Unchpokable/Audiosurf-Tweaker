#include "ui/plugins/static/pins.hxx"

#include "ui/image/svg.hxx"
#include "ui/overlay_config.hxx"
#include "ui/pending_actions.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Renderer-agnostic (no D3D9/GL, no TweakerPlugin PCH) - shared with smoke_test, same convention
// as overlay_state.cxx/texture_cache.cxx (see src/ui/CMakeLists.txt).
namespace
{
constexpr float k_row_h = 28.f;
constexpr float k_row_gap = 10.f;
constexpr float k_margin = 16.f;
constexpr float k_pad_x = 10.f;
constexpr float k_rounding = 6.f;

// 28px row minus 6px of breathing room top and bottom - which is exactly the smallest baked SVG
// size, so the icon lands on the pixel grid instead of being resampled by ImGui.
constexpr float k_icon_size = 16.f;
constexpr float k_icon_gap = 7.f;

struct pin_label {
    std::string text;
    std::string_view icon_key;
    bool quick_player = false;
};

// The bounding box of whatever the last update() drew, for last_rect(). Recorded rather than
// recomputed because the width depends on measured text, and measuring it twice would mean either
// duplicating the layout or paying for it again.
float g_rect[4] { 0.f, 0.f, 0.f, 0.f };
bool g_rect_valid = false;
} // namespace

namespace tw::ui::plugins::statics::pins
{
namespace detail = tw::ui::widgets::detail;

void initialize() noexcept
{
}

void shutdown() noexcept
{
}

void update(const tw::ui::overlay_state::cache& snapshot) noexcept
{
    // Reads through pending_actions rather than the raw snapshot, so a click in the menu shows up
    // here too while its NOTIFY_TWEAK/NOTIFY_SKIN confirmation is still in flight (see
    // ui/pending_actions.hxx).
    std::vector<pin_label> labels;
    for(const auto id : tw::ui::overlay_state::all_tweak_ids()) {
        if(tw::ui::pending_actions::tweak_display_enabled(snapshot, id)) {
            const bool quick_player = tw::ui::overlay_state::is_tweak_quick_player(snapshot, id);
            std::string label = quick_player ? "QP: " : "";
            label += tw::ui::overlay_state::tweak_display_name(id);
            labels.emplace_back(std::move(label), tw::ui::overlay_state::tweak_icon_key(id), quick_player);
        }
    }
    const std::string_view skin_name = tw::ui::pending_actions::skin_display_name(snapshot);
    if(!skin_name.empty()) {
        labels.emplace_back("Skin: " + std::string(skin_name), tw::ui::overlay_state::skin_icon_key(), false);
    }

    g_rect_valid = false;

    if(labels.empty()) {
        return;
    }

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const bool right = overlay_config::pins_side() == overlay_config::side::right;

    // 1 pin -> centered on viewport middle; N pins -> stacked block centered on viewport middle,
    // equally spaced above/below (see Docs/Internal/tweaker-plugin-widgets.md-adjacent spec).
    const float total_h = static_cast<float>(labels.size()) * k_row_h + static_cast<float>(labels.size() - 1) * k_row_gap;
    float y = viewport.y * 0.5f - total_h * 0.5f;

    g_rect[1] = y;
    g_rect[3] = y + total_h;
    // Grown per row below, since each row is only as wide as its own label.
    g_rect[0] = viewport.x;
    g_rect[2] = 0.f;
    g_rect_valid = true;

    for(const auto& label : labels) {
        // Resolved per frame rather than cached with the label: an ImTextureID only survives until
        // the render device is replaced (see ui/image/svg.hxx). Missing icons resolve to
        // ImTextureID_Invalid, and the row simply falls back to the text-only layout.
        const ImTextureID icon = label.icon_key.empty() ? ImTextureID_Invalid : image::svg::get_resource(label.icon_key).sz16;
        const float icon_advance = icon != ImTextureID_Invalid ? k_icon_size + k_icon_gap : 0.f;

        const ImVec2 text_size = ImGui::CalcTextSize(label.text.c_str());
        const float box_w = text_size.x + icon_advance + k_pad_x * 2.f;
        const float x = right ? viewport.x - k_margin - box_w : k_margin;

        const ImVec2 p_min { x, y };
        const ImVec2 p_max { x + box_w, y + k_row_h };

        g_rect[0] = std::min(g_rect[0], p_min.x);
        g_rect[2] = std::max(g_rect[2], p_max.x);

        const ImVec4 bg { theme::surface.x, theme::surface.y, theme::surface.z, theme::surface.w * 0.55f };
        draw->AddRectFilled(p_min, p_max, detail::to_u32(bg), k_rounding);

        const ImVec4 accent = label.quick_player ? theme::accent_secondary : theme::accent_primary;
        const ImU32 text_color = label.quick_player ? detail::to_u32(theme::accent_secondary) : IM_COL32_WHITE;

        if(icon != ImTextureID_Invalid) {
            // Assets are monochrome white, so tinting with the label's own colour is what keeps the
            // icon on-theme (see ui/image/svg.hxx).
            const ImVec2 icon_min { p_min.x + k_pad_x, p_min.y + (k_row_h - k_icon_size) * 0.5f };
            const ImVec2 icon_max { icon_min.x + k_icon_size, icon_min.y + k_icon_size };
            detail::add_image_keep_aspect(draw, icon, ImVec2 { 0.f, 0.f }, icon_min, icon_max, text_color);
        }

        const ImVec2 text_pos { p_min.x + k_pad_x + icon_advance, p_min.y + (k_row_h - text_size.y) * 0.5f };
        detail::add_text_glow(draw, text_pos, label.text.c_str(), text_color, accent, 1.f);

        y += k_row_h + k_row_gap;
    }
}

bool last_rect(float& x0, float& y0, float& x1, float& y1) noexcept
{
    if(!g_rect_valid) {
        return false;
    }

    x0 = g_rect[0];
    y0 = g_rect[1];
    x1 = g_rect[2];
    y1 = g_rect[3];

    return true;
}
} // namespace tw::ui::plugins::statics::pins
