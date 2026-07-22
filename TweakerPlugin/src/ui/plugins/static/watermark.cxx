#include "ui/plugins/static/watermark.hxx"

#include "ui/overlay_config.hxx"
#include "ui/texture_cache.hxx"
#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui.h>

#include <cstdio>

// Renderer-agnostic (no D3D9/GL, no TweakerPlugin PCH) - shared with smoke_test, same convention
// as overlay_state.cxx/texture_cache.cxx (see src/ui/CMakeLists.txt).
namespace
{
constexpr float k_margin = 16.f;
constexpr float k_height = 40.f;
constexpr float k_pad_x = 10.f;
constexpr float k_icon_size = 24.f;
constexpr float k_rounding = 8.f;
// A pre-baked 32px variant, not the 256px master (textures/TweakerIcon-1.png) - minifying that
// 8x+ with no mipmaps at this display size aliased badly (thin bars in the logo lost detail and
// visually read as "squished"). See assets/textures/ for the full size set.
constexpr const char* k_icon_key = "textures/TweakerIcon-5.png";
constexpr const char* k_label = "Audiosurf Tweaker";
} // namespace

namespace tw::ui::plugins::statics::watermark
{
namespace detail = tw::ui::widgets::detail;

void initialize() noexcept
{
}

void shutdown() noexcept
{
}

void update() noexcept
{
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;

    // Opposite corner from notefeed (top-left/top-right), same top margin.
    const bool feed_right = overlay_config::feed_side() == overlay_config::side::right;
    const bool watermark_right = !feed_right;

    char fps_buf[32];
    std::snprintf(fps_buf, sizeof(fps_buf), "%.0f FPS", static_cast<double>(ImGui::GetIO().Framerate));

    const ImVec2 label_size = ImGui::CalcTextSize(k_label);
    const ImVec2 fps_size = ImGui::CalcTextSize(fps_buf);

    const ImTextureID icon = texture_cache::get_or_load(k_icon_key);
    const float icon_slot = icon != ImTextureID_Invalid ? (k_icon_size + 8.f) : 0.f;

    const float content_w = icon_slot + label_size.x + 10.f + fps_size.x;
    const float box_w = content_w + k_pad_x * 2.f;
    const float x = watermark_right ? viewport.x - k_margin - box_w : k_margin;

    const ImVec2 p_min { x, k_margin };
    const ImVec2 p_max { x + box_w, k_margin + k_height };

    detail::add_rect_glow(draw, p_min, p_max, k_rounding, theme::accent_primary, 1.f);

    const ImVec4 bg { theme::surface.x, theme::surface.y, theme::surface.z, theme::surface.w * 0.85f };
    draw->AddRectFilled(p_min, p_max, detail::to_u32(bg), k_rounding);
    draw->AddRect(p_min, p_max, detail::to_u32(theme::accent_border), k_rounding);

    float cursor_x = p_min.x + k_pad_x;
    if(icon != ImTextureID_Invalid) {
        const ImVec2 icon_min { cursor_x, p_min.y + (k_height - k_icon_size) * 0.5f };
        const ImVec2 icon_max { icon_min.x + k_icon_size, icon_min.y + k_icon_size };
        detail::add_image_keep_aspect(draw, icon, ImVec2 { 0.f, 0.f }, icon_min, icon_max);
        cursor_x = icon_max.x + 8.f;
    }

    const ImVec2 label_pos { cursor_x, p_min.y + (k_height - label_size.y) * 0.5f };
    draw->AddText(label_pos, detail::to_u32(theme::text_primary), k_label);
    cursor_x += label_size.x + 10.f;

    const ImVec2 fps_pos { cursor_x, p_min.y + (k_height - fps_size.y) * 0.5f };
    draw->AddText(fps_pos, detail::to_u32(theme::text_muted), fps_buf);
}
} // namespace tw::ui::plugins::statics::watermark
