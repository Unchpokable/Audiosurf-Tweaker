#include "ui/plugins/static/pins.hxx"

#include "ui/overlay_config.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui.h>

#include <string>
#include <vector>

// Renderer-agnostic (no D3D9/GL, no TweakerPlugin PCH) - shared with smoke_test, same convention
// as overlay_state.cxx/texture_cache.cxx (see src/ui/CMakeLists.txt).
namespace tw::ui::plugins::statics::pins
{
namespace detail = tw::ui::widgets::detail;

namespace
{
constexpr float k_row_h = 28.f;
constexpr float k_row_gap = 10.f;
constexpr float k_margin = 16.f;
constexpr float k_pad_x = 10.f;
constexpr float k_rounding = 6.f;
} // namespace

void initialize() noexcept
{
}

void shutdown() noexcept
{
}

void update(const tw::ui::overlay_state::cache& snapshot) noexcept
{
    std::vector<std::string> labels;
    for(const auto id : tw::ui::overlay_state::all_tweak_ids()) {
        if(tw::ui::overlay_state::is_tweak_enabled(snapshot, id)) {
            labels.emplace_back(tw::ui::overlay_state::tweak_display_name(id));
        }
    }
    if(!snapshot.current_skin_name.empty()) {
        labels.push_back("Skin: " + snapshot.current_skin_name);
    }

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

    for(const auto& label : labels) {
        const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
        const float box_w = text_size.x + k_pad_x * 2.f;
        const float x = right ? viewport.x - k_margin - box_w : k_margin;

        const ImVec2 p_min {x, y};
        const ImVec2 p_max {x + box_w, y + k_row_h};

        const ImVec4 bg {theme::surface.x, theme::surface.y, theme::surface.z, theme::surface.w * 0.55f};
        draw->AddRectFilled(p_min, p_max, detail::to_u32(bg), k_rounding);

        const ImVec2 text_pos {p_min.x + k_pad_x, p_min.y + (k_row_h - text_size.y) * 0.5f};
        detail::add_text_glow(draw, text_pos, label.c_str(), IM_COL32_WHITE, theme::accent_primary, 1.f);

        y += k_row_h + k_row_gap;
    }
}
} // namespace tw::ui::plugins::statics::pins
