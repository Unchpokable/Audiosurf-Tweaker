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
    return ImGui::ColorConvertFloat4ToU32(c);
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
    draw->AddImage(ImTextureRef{tex}, p_min, p_max, ImVec2{0.f, 0.f}, ImVec2{1.f, 1.f}, col);
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
} // namespace tw::ui::widgets::detail
