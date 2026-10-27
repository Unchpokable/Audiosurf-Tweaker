// No TweakerPlugin PCH here - part of tweaker_ui, shared with smoke_test (same as ui/widgets/).
#include "ui/kit/paint.hxx"

#include "ui/theme.hxx"

#include <cmath>
#include <string>

namespace
{
// The glows' outermost outline and its line width; glow_reach in the header is what they add up to.
constexpr float k_glow_offset = 6.f;
constexpr float k_glow_thickness = 1.5f;
static_assert(tw::ui::kit::paint::glow_reach == k_glow_offset + k_glow_thickness * 0.5f);

// Pills. Room on both axes: the first pass of the track badges had 6 x 2 and read as text with a tint
// behind it rather than as a pill (Fuckups/tags_too_small.png).
constexpr float k_pill_pad_x = 8.f;
constexpr float k_pill_pad_y = 3.f;

void draw_pill(ImDrawList* draw, const ImVec2& min, const char* text, const ImVec4& ground, const ImVec4& ink) noexcept
{
    const ImVec2 size = tw::ui::kit::paint::pill_size(text);
    draw->AddRectFilled(min, ImVec2 { min.x + size.x, min.y + size.y }, tw::ui::kit::paint::to_u32(ground), tw::ui::theme::radius::pill);
    draw->AddText(ImVec2 { min.x + k_pill_pad_x, min.y + k_pill_pad_y }, tw::ui::kit::paint::to_u32(ink), text);
}

constexpr const char* k_ellipsis = "...";

// How many bytes of `text` fit in `budget` pixels. Found by binary search over byte offsets, backed up to
// a UTF-8 character boundary - a handful of CalcTextSize calls, where walking one character at a time
// would be quadratic on long names.
int fit_length(const char* text, float budget) noexcept
{
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
    return low;
}

// Window chrome.
constexpr float k_title_text_x = 12.f; // the title's inset from the window's left edge
constexpr float k_close_size = 22.f;
constexpr float k_close_gap = 8.f;     // between the end of the title text and the close button
constexpr float k_grip_size = 16.f;
} // namespace

namespace tw::ui::kit::paint
{
ImVec2 aspect_fit_size(ImVec2 src, ImVec2 slot) noexcept
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

void add_image_keep_aspect(
    ImDrawList* draw, ImTextureID tex, ImVec2 src_size, const ImVec2& slot_min, const ImVec2& slot_max, const ImVec4& tint) noexcept
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
    draw->AddImage(ImTextureRef { tex }, p_min, p_max, ImVec2 { 0.f, 0.f }, ImVec2 { 1.f, 1.f }, to_u32(tint));
}

void icon(ImDrawList* draw, ImTextureID tex, ImVec2 min, float px, const ImVec4& tint) noexcept
{
    if(draw == nullptr || tex == ImTextureID_Invalid) {
        return;
    }

    min = ImVec2 { std::floor(min.x), std::floor(min.y) };
    draw->AddImage(ImTextureRef { tex }, min, ImVec2 { min.x + px, min.y + px }, ImVec2 { 0.f, 0.f }, ImVec2 { 1.f, 1.f }, to_u32(tint));
}

void row_background(ImDrawList* draw, const ImVec2& min, const ImVec2& max, float hover_t, float select_t) noexcept
{
    if(draw == nullptr) {
        return;
    }

    ImVec4 fill = lerp(theme::surface_row, theme::surface_row_hover, hover_t);
    fill = lerp(fill, theme::accent_selected, select_t);
    draw->AddRectFilled(min, max, to_u32(fill), theme::radius::row);

    if(select_t > 0.01f) {
        draw->AddRect(min, max, to_u32(lerp(theme::border, theme::accent_border, select_t)), theme::radius::row);
    }
}

bool scroll_body_begin(const char* id, ImVec2 size, float padding, float rounding, ImGuiWindowFlags flags)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::surface);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2 { padding, padding });
    return ImGui::BeginChild(id, size, ImGuiChildFlags_Borders, flags);
}

void scroll_body_end()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void color_swatch(ImDrawList* draw, const ImVec2& center, float diameter, const ImVec4& color, float hover_t) noexcept
{
    const float ring = diameter * 0.5f;
    const float fill = ring - 1.5f;

    // The glow forces full alpha: several theme colours are deliberately faint tints (accent_ghost,
    // accent_soft), and their glow would be as faint as they are.
    add_circle_glow(draw, center, ring, ImVec4 { color.x, color.y, color.z, 1.f }, hover_t);

    if(color.w < 0.999f) {
        // ImGui's RenderColorRectWithAlphaCheckerboard rounds only the cells in the rect's corners, and
        // along a circle's arc every cell would poke out. So: a light disc, and dark cells only where the
        // whole cell is inside it - the rim stays light, which reads as part of the pattern.
        constexpr float k_cell = 4.f;
        draw->AddCircleFilled(center, fill, to_u32(theme::text_secondary));

        const ImU32 dark = to_u32(theme::text_faint);
        const float r2 = fill * fill;
        const ImVec2 origin { std::floor(center.x - fill), std::floor(center.y - fill) };
        const int cells = static_cast<int>(std::ceil(fill * 2.f / k_cell));
        for(int row = 0; row < cells; ++row) {
            for(int column = row % 2; column < cells; column += 2) {
                const ImVec2 a { origin.x + static_cast<float>(column) * k_cell, origin.y + static_cast<float>(row) * k_cell };
                const ImVec2 b { a.x + k_cell, a.y + k_cell };
                const auto inside = [&](float x, float y) {
                    return (x - center.x) * (x - center.x) + (y - center.y) * (y - center.y) <= r2;
                };
                if(inside(a.x, a.y) && inside(b.x, a.y) && inside(a.x, b.y) && inside(b.x, b.y)) {
                    draw->AddRectFilled(a, b, dark);
                }
            }
        }
    }

    draw->AddCircleFilled(center, fill, to_u32(color));
    draw->AddCircle(center, ring, to_u32(theme::border));
}

ImRect label_row(const char* label, const ImVec2& control)
{
    constexpr float k_gap = 8.f; // between the label's end and the control

    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float font = ImGui::GetFontSize();
    const float row_h = (std::max)(control.y, font);
    const float label_w = (std::max)(0.f, avail - control.x);

    const ImVec2 text_pos { start.x, start.y + std::floor((row_h - font) * 0.5f) };
    const float text_max = start.x + label_w - k_gap;
    add_text_ellipsis(ImGui::GetWindowDrawList(), text_pos, text_max, to_u32(theme::text_primary), label);
    const float text_w = (std::min)(ImGui::CalcTextSize(label).x, (std::max)(0.f, text_max - start.x));

    // The label's part of the row as an item, then the control on the same line: ImGui's line height then
    // covers both, whichever is taller.
    ImGui::Dummy(ImVec2 { label_w, row_h });
    ImGui::SameLine(0.f, 0.f);
    ImGui::SetCursorScreenPos(ImVec2 { start.x + label_w, start.y + std::floor((row_h - control.y) * 0.5f) });

    return ImRect { text_pos, ImVec2 { text_pos.x + text_w, text_pos.y + font } };
}

ImRect label_block(std::string_view label, const ImVec2& control)
{
    constexpr float k_gap = 8.f; // between the label's longest line and the control - as label_row

    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float avail = ImGui::GetContentRegionAvail().x;
    const float font = ImGui::GetFontSize();
    const float first_h = (std::max)(control.y, font);
    const float label_w = (std::max)(0.f, avail - control.x);
    const float wrap_w = (std::max)(1.f, label_w - k_gap);

    const char* begin = label.data();
    const char* end = begin + label.size();
    const ImVec2 text_size = ImGui::CalcTextSize(begin, end, false, wrap_w);
    const ImVec2 text_pos { start.x, start.y + std::floor((first_h - font) * 0.5f) };
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), font, text_pos, to_u32(theme::text_primary), begin, end, wrap_w);

    // As label_row: the label's part as an item, the control on the same line - ImGui's line height then
    // covers the taller of the two, here the whole wrapped block.
    const float block_h = (std::max)(first_h, text_pos.y - start.y + text_size.y);
    ImGui::Dummy(ImVec2 { label_w, block_h });
    ImGui::SameLine(0.f, 0.f);
    ImGui::SetCursorScreenPos(ImVec2 { start.x + label_w, start.y + std::floor((first_h - control.y) * 0.5f) });

    return ImRect { text_pos, ImVec2 { text_pos.x + text_size.x, text_pos.y + text_size.y } };
}

void paragraph(std::string_view text, const ImVec4& color)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float wrap_w = (std::max)(1.f, ImGui::GetContentRegionAvail().x);
    const float font = ImGui::GetFontSize();

    const char* begin = text.data();
    const char* end = begin + text.size();
    const ImVec2 size = text.empty() ? ImVec2 { 0.f, font } : ImGui::CalcTextSize(begin, end, false, wrap_w);
    if(!text.empty()) {
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), font, start, to_u32(color), begin, end, wrap_w);
    }

    ImGui::Dummy(ImVec2 { (std::max)(1.f, size.x), (std::max)(font, size.y) });
}

ImVec2 pill_size(const char* text) noexcept
{
    return ImVec2 { ImGui::CalcTextSize(text).x + k_pill_pad_x * 2.f, pill_height() };
}

float pill_height() noexcept
{
    return ImGui::GetFontSize() + k_pill_pad_y * 2.f;
}

void pill(ImDrawList* draw, const ImVec2& min, const char* text) noexcept
{
    draw_pill(draw, min, text, theme::surface_badge, theme::text_muted);
}

void pill(ImDrawList* draw, const ImVec2& min, const char* text, const ImVec4& tint) noexcept
{
    ImVec4 ground = tint;
    ground.w *= 0.18f;
    draw_pill(draw, min, text, ground, tint);
}

ImRect window_begin(const char* name, const char* title, bool closable)
{
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove
                                       | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
                                       | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    // NoBackground only skips ImGui's own WindowBg fill - it still draws its straight-cornered Border
    // outline and, unless NoNav, a keyboard-nav highlight rect, and both peek out from under the rounded
    // corners as a stray gray edge. Belt and suspenders: the border's colour is zeroed as well as its size.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme::radius::window);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4 { 0.f, 0.f, 0.f, 0.f });
    ImGui::Begin(name, nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 max { min.x + size.x, min.y + size.y };

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(min, max, to_u32(theme::surface), theme::radius::window);
    draw->AddRect(min, max, to_u32(theme::border_window), theme::radius::window);

    const ImRect bar { min, ImVec2 { max.x, min.y + theme::space::title_h } };
    draw->AddRectFilled(bar.Min, bar.Max, to_u32(theme::surface_elevated), theme::radius::window, ImDrawFlags_RoundCornersTop);

    const float text_end = closable ? close_button_rect(bar).Min.x - k_close_gap : bar.Max.x - theme::space::window_pad;
    add_text_ellipsis(draw,
        ImVec2 { bar.Min.x + k_title_text_x, bar.Min.y + (theme::space::title_h - ImGui::GetFontSize()) * 0.5f },
        text_end,
        to_u32(theme::text_primary),
        title);

    return bar;
}

ImRect close_button_rect(const ImRect& title) noexcept
{
    const ImVec2 min { title.Max.x - theme::space::window_pad - k_close_size, title.Min.y + (title.GetHeight() - k_close_size) * 0.5f };
    return ImRect { min, ImVec2 { min.x + k_close_size, min.y + k_close_size } };
}

// Two strokes rather than an SVG: there is no close.svg in assets/icons, and one shape used once does not
// need to become an asset.
void close_button(ImDrawList* draw, const ImRect& slot, float hover_t) noexcept
{
    const ImVec2 center = slot.GetCenter();
    if(hover_t > 0.01f) {
        ImVec4 disc = theme::surface_hover;
        disc.w *= hover_t;
        draw->AddCircleFilled(center, slot.GetWidth() * 0.5f, to_u32(disc));
    }

    const ImU32 col = to_u32(lerp(theme::text_muted, theme::text_primary, hover_t));
    constexpr float k_arm = 4.5f;
    draw->AddLine(ImVec2 { center.x - k_arm, center.y - k_arm }, ImVec2 { center.x + k_arm, center.y + k_arm }, col, 1.6f);
    draw->AddLine(ImVec2 { center.x - k_arm, center.y + k_arm }, ImVec2 { center.x + k_arm, center.y - k_arm }, col, 1.6f);
}

ImRect resize_grip_rect(const ImVec2& window_max) noexcept
{
    return ImRect { ImVec2 { window_max.x - k_grip_size, window_max.y - k_grip_size }, window_max };
}

// Arcs concentric with the window's own corner. The corner's free space is the band between the window's
// rounding and the rounding of the panel inside it, one padding in - straight dashes across it either poked
// out past the window's silhouette or ran under the panel as soon as the radii grew.
void resize_grip(ImDrawList* draw, const ImVec2& window_max) noexcept
{
    constexpr float k_trim = 0.2f; // radians off each end, so the arcs do not touch the straight edges
    const float r = theme::radius::window;
    const ImVec2 center { window_max.x - r, window_max.y - r };
    const ImU32 col = to_u32(theme::border_strong);
    for(const float inset : { 3.f, 6.f }) {
        if(r - inset < 2.f) {
            break;
        }
        draw->PathArcTo(center, r - inset, k_trim, IM_PI * 0.5f - k_trim, 8);
        draw->PathStroke(col, ImDrawFlags_None, 1.5f);
    }
}

// ImGui's own RenderTextEllipsis is internal and takes its colour from the style stack rather than an
// argument, which does not fit how everything else here is drawn. The cut point: fit_length().
bool add_text_ellipsis(ImDrawList* draw, const ImVec2& pos, float max_x, ImU32 col, const char* text) noexcept
{
    if(draw == nullptr || text == nullptr || text[0] == '\0') {
        return false;
    }

    const float avail = max_x - pos.x;
    if(avail <= 0.f) {
        return true;
    }

    if(ImGui::CalcTextSize(text).x <= avail) {
        draw->AddText(pos, col, text);
        return false;
    }

    const float budget = avail - ImGui::CalcTextSize(k_ellipsis).x;
    if(budget <= 0.f) {
        return true;
    }

    const int low = fit_length(text, budget);
    draw->AddText(pos, col, text, text + low);
    draw->AddText(ImVec2 { pos.x + ImGui::CalcTextSize(text, text + low).x, pos.y }, col, k_ellipsis);
    return true;
}

std::string fit_text(const char* text, float max_w)
{
    if(text == nullptr || ImGui::CalcTextSize(text).x <= max_w) {
        return text != nullptr ? std::string(text) : std::string();
    }

    const float budget = max_w - ImGui::CalcTextSize(k_ellipsis).x;
    if(budget <= 0.f) {
        return std::string();
    }

    std::string out(text, static_cast<std::size_t>(fit_length(text, budget)));
    out += k_ellipsis;
    return out;
}

void add_text_glow(ImDrawList* draw,
    const ImVec2& pos,
    const char* text,
    ImU32 text_col,
    ImVec4 glow_col,
    float strength,
    ImFont* font,
    float font_size) noexcept
{
    if(draw == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }

    if(font == nullptr) {
        font = ImGui::GetFont();
    }
    if(font_size <= 0.f) {
        font_size = ImGui::GetFontSize();
    }
    if(font == nullptr) {
        return;
    }

    strength = std::clamp(strength, 0.f, 1.f);
    if(strength > 0.01f) {
        const float glow_a = glow_col.w * strength * 0.15f;
        const ImU32 glow_u32 = to_u32(ImVec4 { glow_col.x, glow_col.y, glow_col.z, glow_a });
        // Scaled with the text: a one-pixel spread around 13px type is a halo, around 40px type it
        // is a slightly thicker outline and reads as an accident.
        const float k_off = std::max(1.f, font_size * 0.06f);
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
            draw->AddText(font, font_size, ImVec2 { pos.x + o.x, pos.y + o.y }, glow_u32, text);
        }
    }

    draw->AddText(font, font_size, pos, text_col, text);
}

void add_rect_glow(ImDrawList* draw, const ImVec2& p_min, const ImVec2& p_max, float rounding, ImVec4 glow_col, float strength) noexcept
{
    if(draw == nullptr) {
        return;
    }

    strength = std::clamp(strength, 0.f, 1.f);
    if(strength <= 0.01f) {
        return;
    }

    constexpr int k_layers = 4;
    for(int i = k_layers; i >= 1; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(k_layers);
        const float offset = k_glow_offset * t;
        const float a = glow_col.w * strength * 0.16f * (1.f - t) + glow_col.w * strength * 0.03f;
        const ImU32 col = to_u32(ImVec4 { glow_col.x, glow_col.y, glow_col.z, a });
        draw->AddRect(ImVec2 { p_min.x - offset, p_min.y - offset },
            ImVec2 { p_max.x + offset, p_max.y + offset },
            col,
            rounding + offset,
            0,
            k_glow_thickness);
    }
}

void add_circle_glow(ImDrawList* draw, const ImVec2& center, float radius, ImVec4 glow_col, float strength) noexcept
{
    if(draw == nullptr) {
        return;
    }

    strength = std::clamp(strength, 0.f, 1.f);
    if(strength <= 0.01f) {
        return;
    }

    constexpr int k_layers = 4;
    for(int i = k_layers; i >= 1; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(k_layers);
        const float offset = k_glow_offset * t;
        const float a = glow_col.w * strength * 0.16f * (1.f - t) + glow_col.w * strength * 0.03f;
        const ImU32 col = to_u32(ImVec4 { glow_col.x, glow_col.y, glow_col.z, a });
        draw->AddCircle(center, radius + offset, col, 0, k_glow_thickness);
    }
}

bool add_gradient_polyline(
    ImDrawList* draw, const ImVec2* points, const ImU32* colors, int count, float thickness, ImDrawFlags flags) noexcept
{
    if(draw == nullptr || points == nullptr || colors == nullptr || count < 2) {
        return true;
    }

    const int start = draw->VtxBuffer.Size;
    draw->AddPolyline(points, count, IM_COL32_WHITE, thickness, flags);
    const int added = draw->VtxBuffer.Size - start;
    if(added <= 0) {
        return true;
    }

    // The stroke was drawn white, so a vertex's own colour is white at the alpha its anti-aliasing gave it
    // (opaque in the middle, clear at the fringe): the point's colour with its alpha scaled by that.
    ImDrawVert* vertices = draw->VtxBuffer.Data + start;
    const auto tint = [](ImDrawVert& vertex, ImU32 color) {
        const ImU32 coverage = (vertex.col >> IM_COL32_A_SHIFT) & 0xFF;
        const ImU32 alpha = (((color >> IM_COL32_A_SHIFT) & 0xFF) * coverage + 127) / 255;
        vertex.col = (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
    };

    if((draw->Flags & ImDrawListFlags_AntiAliasedLines) != 0) {
        const int per_point = added / count;
        if(per_point * count == added && per_point >= 2 && per_point <= 4) {
            for(int i = 0; i < count; ++i) {
                for(int k = 0; k < per_point; ++k) {
                    tint(vertices[i * per_point + k], colors[i]);
                }
            }
            return true;
        }
    }
    else {
        const bool closed = (flags & ImDrawFlags_Closed) != 0;
        const int segments = closed ? count : count - 1;
        if(added == segments * 4) {
            for(int s = 0; s < segments; ++s) {
                const ImU32 from = colors[s];
                const ImU32 to = colors[(s + 1) % count];
                tint(vertices[s * 4 + 0], from);
                tint(vertices[s * 4 + 1], to);
                tint(vertices[s * 4 + 2], to);
                tint(vertices[s * 4 + 3], from);
            }
            return true;
        }
    }

    for(int i = 0; i < added; ++i) {
        tint(vertices[i], colors[0]);
    }
    return false;
}
} // namespace tw::ui::kit::paint
