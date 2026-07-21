#pragma once

#include <imgui.h>

#include <algorithm>
#include <cstdint>

namespace tw::ui::widgets::detail
{
inline std::int32_t dt_ms() noexcept
{
    const float ms = ImGui::GetIO().DeltaTime * 1000.f;
    return std::max(std::int32_t{1}, static_cast<std::int32_t>(ms + 0.5f));
}

inline ImVec4 lerp(const ImVec4& a, const ImVec4& b, float t) noexcept
{
    t = std::clamp(t, 0.f, 1.f);
    return ImVec4{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t,
    };
}

inline ImU32 to_u32(const ImVec4& c) noexcept
{
    // Match ImGui::GetColorU32 — respect stacked StyleVar_Alpha (tab_view crossfade).
    ImVec4 m = c;
    m.w *= ImGui::GetStyle().Alpha;
    return ImGui::ColorConvertFloat4ToU32(m);
}

inline ImU32 apply_style_alpha(ImU32 col) noexcept
{
    const float a = ImGui::GetStyle().Alpha;
    if (a >= 0.999f)
        return col;
    ImVec4 f = ImGui::ColorConvertU32ToFloat4(col);
    f.w *= a;
    return ImGui::ColorConvertFloat4ToU32(f);
}

inline void add_rect_filled_rounded(
    ImDrawList* draw,
    const ImVec2& p_min,
    const ImVec2& p_max,
    ImU32 col,
    float rounding) noexcept
{
    draw->AddRectFilled(p_min, p_max, col, rounding);
}

// Fit `src` into `slot` preserving aspect ratio (letterbox / pillarbox).
inline ImVec2 aspect_fit_size(ImVec2 src, ImVec2 slot) noexcept
{
    if (src.x <= 0.f || src.y <= 0.f)
        return slot;
    if (slot.x <= 0.f || slot.y <= 0.f)
        return ImVec2{0.f, 0.f};

    const float scale = std::min(slot.x / src.x, slot.y / src.y);
    return ImVec2{src.x * scale, src.y * scale};
}

inline ImVec2 aspect_fit_centered(ImVec2 src, const ImVec2& slot_min, const ImVec2& slot_max) noexcept
{
    const ImVec2 slot{slot_max.x - slot_min.x, slot_max.y - slot_min.y};
    return aspect_fit_size(src, slot);
}

inline void add_image_keep_aspect(
    ImDrawList* draw,
    ImTextureID tex,
    ImVec2 src_size,
    const ImVec2& slot_min,
    const ImVec2& slot_max,
    ImU32 col = IM_COL32_WHITE) noexcept
{
    if (tex == ImTextureID_Invalid)
        return;

    const ImVec2 slot{slot_max.x - slot_min.x, slot_max.y - slot_min.y};
    if (src_size.x <= 0.f || src_size.y <= 0.f)
        src_size = slot;

    const ImVec2 fitted = aspect_fit_size(src_size, slot);
    const ImVec2 p_min{
        slot_min.x + (slot.x - fitted.x) * 0.5f,
        slot_min.y + (slot.y - fitted.y) * 0.5f,
    };
    const ImVec2 p_max{p_min.x + fitted.x, p_min.y + fitted.y};
    draw->AddImage(ImTextureRef{tex}, p_min, p_max, ImVec2{0.f, 0.f}, ImVec2{1.f, 1.f}, apply_style_alpha(col));
}

inline ImVec2 resolve_size(ImVec2 size, float default_w, float default_h) noexcept
{
    if (size.x <= 0.f)
        size.x = ImGui::GetContentRegionAvail().x;
    if (size.x <= 0.f)
        size.x = default_w;
    if (size.y <= 0.f)
        size.y = default_h;
    return size;
}

// Soft glow via offset AddText copies (no shaders). strength in [0, 1].
inline void add_text_glow(
    ImDrawList* draw,
    const ImVec2& pos,
    const char* text,
    ImU32 text_col,
    ImVec4 glow_col,
    float strength) noexcept
{
    if (draw == nullptr || text == nullptr || text[0] == '\0')
        return;

    strength = std::clamp(strength, 0.f, 1.f);
    if (strength > 0.01f)
    {
        const float glow_a = glow_col.w * strength * 0.15f;
        const ImU32 glow_u32 = to_u32(ImVec4{glow_col.x, glow_col.y, glow_col.z, glow_a});
        constexpr float k_off = 1.f;
        const ImVec2 offsets[] = {
            {-k_off, 0.f},
            {k_off, 0.f},
            {0.f, -k_off},
            {0.f, k_off},
            {-k_off, -k_off},
            {k_off, -k_off},
            {-k_off, k_off},
            {k_off, k_off},
        };
        for (const ImVec2& o : offsets)
            draw->AddText(ImVec2{pos.x + o.x, pos.y + o.y}, glow_u32, text);
    }

    draw->AddText(pos, text_col, text);
}

// Soft glow around a rounded rect border via several expanding, fading outline copies (no
// shaders) - same technique as add_text_glow above. strength in [0, 1].
inline void add_rect_glow(
    ImDrawList* draw,
    const ImVec2& p_min,
    const ImVec2& p_max,
    float rounding,
    ImVec4 glow_col,
    float strength) noexcept
{
    if (draw == nullptr)
        return;

    strength = std::clamp(strength, 0.f, 1.f);
    if (strength <= 0.01f)
        return;

    constexpr int k_layers = 4;
    constexpr float k_max_offset = 6.f;
    for (int i = k_layers; i >= 1; --i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(k_layers);
        const float offset = k_max_offset * t;
        const float a = glow_col.w * strength * 0.16f * (1.f - t) + glow_col.w * strength * 0.03f;
        const ImU32 col = to_u32(ImVec4{glow_col.x, glow_col.y, glow_col.z, a});
        draw->AddRect(
            ImVec2{p_min.x - offset, p_min.y - offset},
            ImVec2{p_max.x + offset, p_max.y + offset},
            col,
            rounding + offset,
            0,
            1.5f);
    }
}
} // namespace tw::ui::widgets::detail
