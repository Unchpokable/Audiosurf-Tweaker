#include "ui/plugins/static/notefeed.hxx"

#include "ui/image/svg.hxx"
#include "ui/overlay_config.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <algorithm>
#include <string>
#include <vector>

// Renderer-agnostic (no D3D9/GL, no TweakerPlugin PCH) - shared with smoke_test, same convention
// as overlay_state.cxx/texture_cache.cxx (see src/ui/CMakeLists.txt).
namespace
{
constexpr float k_lifetime_ms = 3000.f;
constexpr float k_fade_in_ms = 200.f;
constexpr float k_fade_out_ms = 400.f;
constexpr float k_reflow_ms = 220.f;
constexpr float k_row_h = 44.f;
constexpr float k_row_gap = 8.f;
constexpr float k_margin = 16.f;
constexpr float k_width = 300.f;
constexpr float k_rounding = 8.f;

struct entry {
    std::string text;
    // The resource key, not a resolved ImTextureID: a toast can easily outlive a device change, and
    // the texture handle would be dangling by then (see ui/image/svg.hxx). Resolved per frame in
    // update() instead - a hash lookup.
    std::string icon_key;
    double spawn_time_ms = 0.0;
    float y_current = 0.f; // source of truth, written by y_tween.step() - see button.cxx convention
    tweeny::tween<float> y_tween;
};

std::vector<entry> g_entries;
double g_clock_ms = 0.0;

void retarget_positions()
{
    for(std::size_t i = 0; i < g_entries.size(); ++i) {
        const float target = static_cast<float>(i) * (k_row_h + k_row_gap);
        g_entries[i].y_tween =
            tweeny::from(g_entries[i].y_current).to(target).during(static_cast<int>(k_reflow_ms)).via(tweeny::easing::cubicOut);
    }
}
} // namespace

namespace tw::ui::plugins::statics::notefeed
{
namespace detail = tw::ui::widgets::detail;

void initialize() noexcept
{
    g_entries.clear();
    g_clock_ms = 0.0;
}

void shutdown() noexcept
{
    g_entries.clear();
}

void push(std::string_view text, std::string_view icon_resource_key)
{
    entry e;
    e.text.assign(text);
    e.icon_key.assign(icon_resource_key);
    e.spawn_time_ms = g_clock_ms;
    e.y_current = static_cast<float>(g_entries.size()) * (k_row_h + k_row_gap);
    g_entries.push_back(std::move(e));
}

void update() noexcept
{
    const auto dt = detail::dt_ms();
    g_clock_ms += dt;

    bool removed_any = false;
    for(std::size_t i = 0; i < g_entries.size();) {
        const float age = static_cast<float>(g_clock_ms - g_entries[i].spawn_time_ms);
        if(age >= k_lifetime_ms) {
            g_entries.erase(g_entries.begin() + static_cast<std::ptrdiff_t>(i));
            removed_any = true;
            continue;
        }
        ++i;
    }
    if(removed_any) {
        retarget_positions();
    }

    for(entry& e : g_entries) {
        if(!e.y_tween.isFinished()) {
            e.y_current = e.y_tween.step(dt);
        }
    }

    if(g_entries.empty()) {
        return;
    }

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const bool right = overlay_config::feed_side() == overlay_config::side::right;
    const float origin_x = right ? viewport.x - k_margin - k_width : k_margin;

    for(const entry& e : g_entries) {
        const float age = static_cast<float>(g_clock_ms - e.spawn_time_ms);
        float alpha;
        if(age < k_fade_in_ms) {
            alpha = age / k_fade_in_ms;
        }
        else if(age > k_lifetime_ms - k_fade_out_ms) {
            alpha = std::clamp((k_lifetime_ms - age) / k_fade_out_ms, 0.f, 1.f);
        }
        else {
            alpha = 1.f;
        }

        const float top = k_margin + e.y_current;
        const ImVec2 p_min { origin_x, top };
        const ImVec2 p_max { origin_x + k_width, top + k_row_h };

        const ImVec4 bg { theme::surface.x, theme::surface.y, theme::surface.z, theme::surface.w * 0.92f * alpha };
        const ImVec4 border { theme::border.x, theme::border.y, theme::border.z, theme::border.w * alpha };
        draw->AddRectFilled(p_min, p_max, detail::to_u32(bg), k_rounding);
        draw->AddRect(p_min, p_max, detail::to_u32(border), k_rounding);

        // The icon slot is 32px (k_row_h - 12), which is exactly the largest baked SVG size.
        const ImTextureID icon = e.icon_key.empty() ? ImTextureID_Invalid : image::svg::get_resource(e.icon_key).sz32;

        float text_x = p_min.x + 12.f;
        if(icon != ImTextureID_Invalid) {
            const ImVec2 icon_min { p_min.x + 8.f, p_min.y + 6.f };
            const ImVec2 icon_max { icon_min.x + (k_row_h - 12.f), p_max.y - 6.f };
            // Assets are monochrome white - the tint is what carries the fade here.
            const ImU32 icon_col = IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.f + 0.5f));
            detail::add_image_keep_aspect(draw, icon, ImVec2 { 0.f, 0.f }, icon_min, icon_max, icon_col);
            text_x = icon_max.x + 8.f;
        }

        const ImVec4 text_col { theme::text_primary.x, theme::text_primary.y, theme::text_primary.z, theme::text_primary.w * alpha };
        const ImVec2 text_pos { text_x, p_min.y + (k_row_h - ImGui::GetTextLineHeight()) * 0.5f };
        draw->AddText(text_pos, detail::to_u32(text_col), e.text.c_str());
    }
}
} // namespace tw::ui::plugins::statics::notefeed
