#pragma once

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace tw::ui::widgets::detail
{
// Frame delta in whole milliseconds, which is the unit tweeny steps in.
//
// The fractional part is carried into the next frame rather than rounded away, and this matters a
// lot more than it looks: the previous version clamped to a minimum of 1 ms, so above ~1000 FPS
// every frame claimed a full millisecond had passed when a fraction of one had. Every tween then
// ran at (real FPS / 1000)x speed - at the several thousand FPS an uncapped window reaches, a
// 150 ms hover finishes in tens of milliseconds and a 90 ms press is simply not visible. It reads
// exactly like the animations were removed. Carrying the remainder makes the total elapsed time
// exact at any frame rate, and returning 0 on a sub-millisecond frame is correct - the tween just
// does not advance that frame.
//
// Computed once per frame and cached: this is called by every animated widget, and a shared carry
// consumed per call would hand the first caller the whole remainder and the rest nothing.
inline std::int32_t dt_ms() noexcept
{
    static int last_frame = -1;
    static std::int32_t frame_dt = 0;
    static float carry = 0.f;

    const int frame = ImGui::GetFrameCount();
    if(frame != last_frame) {
        last_frame = frame;

        const float ms = ImGui::GetIO().DeltaTime * 1000.f + carry;
        frame_dt = static_cast<std::int32_t>(ms);
        carry = ms - static_cast<float>(frame_dt);
    }

    return frame_dt;
}

inline ImVec4 lerp(const ImVec4& a, const ImVec4& b, float t) noexcept
{
    t = std::clamp(t, 0.f, 1.f);
    return ImVec4 {
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
    if(a >= 0.999f) {
        return col;
    }
    ImVec4 f = ImGui::ColorConvertU32ToFloat4(col);
    f.w *= a;
    return ImGui::ColorConvertFloat4ToU32(f);
}

inline void add_rect_filled_rounded(ImDrawList* draw, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding) noexcept
{
    draw->AddRectFilled(p_min, p_max, col, rounding);
}

// Fit `src` into `slot` preserving aspect ratio (letterbox / pillarbox).
inline ImVec2 aspect_fit_size(ImVec2 src, ImVec2 slot) noexcept
{
    if(src.x <= 0.f || src.y <= 0.f) {
        return slot;
    }
    if(slot.x <= 0.f || slot.y <= 0.f) {
        return ImVec2 { 0.f, 0.f };
    }

    const float scale = std::min(slot.x / src.x, slot.y / src.y);
    return ImVec2 { src.x * scale, src.y * scale };
}

inline ImVec2 aspect_fit_centered(ImVec2 src, const ImVec2& slot_min, const ImVec2& slot_max) noexcept
{
    const ImVec2 slot { slot_max.x - slot_min.x, slot_max.y - slot_min.y };
    return aspect_fit_size(src, slot);
}

inline void add_image_keep_aspect(
    ImDrawList* draw, ImTextureID tex, ImVec2 src_size, const ImVec2& slot_min, const ImVec2& slot_max, ImU32 col = IM_COL32_WHITE) noexcept
{
    if(tex == ImTextureID_Invalid) {
        return;
    }

    const ImVec2 slot { slot_max.x - slot_min.x, slot_max.y - slot_min.y };
    if(src_size.x <= 0.f || src_size.y <= 0.f) {
        src_size = slot;
    }

    const ImVec2 fitted = aspect_fit_size(src_size, slot);
    const ImVec2 p_min {
        slot_min.x + (slot.x - fitted.x) * 0.5f,
        slot_min.y + (slot.y - fitted.y) * 0.5f,
    };
    const ImVec2 p_max { p_min.x + fitted.x, p_min.y + fitted.y };
    draw->AddImage(ImTextureRef { tex }, p_min, p_max, ImVec2 { 0.f, 0.f }, ImVec2 { 1.f, 1.f }, apply_style_alpha(col));
}

// Text at an explicit pixel size, independent of the window's font size - for the secondary lines
// that want to be visibly smaller (a playlist's track count under its name). One font is baked, so
// this scales it rather than switching face.
inline void add_text_scaled(ImDrawList* draw, float font_size, const ImVec2& pos, ImU32 col, const char* text) noexcept
{
    if(draw == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    draw->AddText(ImGui::GetFont(), font_size, pos, col, text);
}

// Draws `text` clipped to end at `max_x`, ending in an ellipsis when it does not fit. ImGui's own
// RenderTextEllipsis is internal and takes its colour from the style stack rather than an argument,
// which does not fit how everything else here is drawn.
//
// The cut point is found by binary search over byte offsets and then backed up to a UTF-8 character
// boundary - a handful of CalcTextSize calls per truncated string, and only for rows actually on
// screen. Walking one character at a time would be quadratic on long names.
inline void add_text_ellipsis(ImDrawList* draw, const ImVec2& pos, float max_x, ImU32 col, const char* text) noexcept
{
    if(draw == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    const float avail = max_x - pos.x;
    if(avail <= 0.f) {
        return;
    }

    if(ImGui::CalcTextSize(text).x <= avail) {
        draw->AddText(pos, col, text);
        return;
    }

    static constexpr const char* k_ellipsis = "...";
    const float budget = avail - ImGui::CalcTextSize(k_ellipsis).x;
    if(budget <= 0.f) {
        return;
    }

    const auto length = static_cast<int>(std::char_traits<char>::length(text));
    int low = 0;
    int high = length;
    while(low < high) {
        int mid = (low + high + 1) / 2;
        // Never split a multi-byte character: continuation bytes are 10xxxxxx.
        while(mid > 0 && (static_cast<unsigned char>(text[mid]) & 0xC0) == 0x80) {
            --mid;
        }
        if(mid <= low) {
            break;
        }

        if(ImGui::CalcTextSize(text, text + mid).x <= budget) {
            low = mid;
        }
        else {
            high = mid - 1;
        }
    }

    draw->AddText(pos, col, text, text + low);
    draw->AddText(ImVec2 { pos.x + ImGui::CalcTextSize(text, text + low).x, pos.y }, col, k_ellipsis);
}

inline ImVec2 resolve_size(ImVec2 size, float default_w, float default_h) noexcept
{
    if(size.x <= 0.f) {
        size.x = ImGui::GetContentRegionAvail().x;
    }
    if(size.x <= 0.f) {
        size.x = default_w;
    }
    if(size.y <= 0.f) {
        size.y = default_h;
    }
    return size;
}

// Soft glow via offset AddText copies (no shaders). strength in [0, 1].
inline void add_text_glow(ImDrawList* draw, const ImVec2& pos, const char* text, ImU32 text_col, ImVec4 glow_col, float strength) noexcept
{
    if(draw == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    strength = std::clamp(strength, 0.f, 1.f);
    if(strength > 0.01f) {
        const float glow_a = glow_col.w * strength * 0.15f;
        const ImU32 glow_u32 = to_u32(ImVec4 { glow_col.x, glow_col.y, glow_col.z, glow_a });
        constexpr float k_off = 1.f;
        const ImVec2 offsets[] = {
            { -k_off, 0.f },
            { k_off, 0.f },
            { 0.f, -k_off },
            { 0.f, k_off },
            { -k_off, -k_off },
            { k_off, -k_off },
            { -k_off, k_off },
            { k_off, k_off },
        };
        for(const ImVec2& o : offsets) {
            draw->AddText(ImVec2 { pos.x + o.x, pos.y + o.y }, glow_u32, text);
        }
    }

    draw->AddText(pos, text_col, text);
}

// Soft glow around a rounded rect border via several expanding, fading outline copies (no
// shaders) - same technique as add_text_glow above. strength in [0, 1].
inline void add_rect_glow(
    ImDrawList* draw, const ImVec2& p_min, const ImVec2& p_max, float rounding, ImVec4 glow_col, float strength) noexcept
{
    if(draw == nullptr) {
        return;
    }

    strength = std::clamp(strength, 0.f, 1.f);
    if(strength <= 0.01f) {
        return;
    }

    constexpr int k_layers = 4;
    constexpr float k_max_offset = 6.f;
    for(int i = k_layers; i >= 1; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(k_layers);
        const float offset = k_max_offset * t;
        const float a = glow_col.w * strength * 0.16f * (1.f - t) + glow_col.w * strength * 0.03f;
        const ImU32 col = to_u32(ImVec4 { glow_col.x, glow_col.y, glow_col.z, a });
        draw->AddRect(
            ImVec2 { p_min.x - offset, p_min.y - offset }, ImVec2 { p_max.x + offset, p_max.y + offset }, col, rounding + offset, 0, 1.5f);
    }
}

// Soft glow around a circle via several expanding, fading outline copies (no shaders) - same
// technique as add_rect_glow above. strength in [0, 1].
inline void add_circle_glow(ImDrawList* draw, const ImVec2& center, float radius, ImVec4 glow_col, float strength) noexcept
{
    if(draw == nullptr) {
        return;
    }

    strength = std::clamp(strength, 0.f, 1.f);
    if(strength <= 0.01f) {
        return;
    }

    constexpr int k_layers = 4;
    constexpr float k_max_offset = 6.f;
    for(int i = k_layers; i >= 1; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(k_layers);
        const float offset = k_max_offset * t;
        const float a = glow_col.w * strength * 0.16f * (1.f - t) + glow_col.w * strength * 0.03f;
        const ImU32 col = to_u32(ImVec4 { glow_col.x, glow_col.y, glow_col.z, a });
        draw->AddCircle(center, radius + offset, col, 0, 1.5f);
    }
}
} // namespace tw::ui::widgets::detail
