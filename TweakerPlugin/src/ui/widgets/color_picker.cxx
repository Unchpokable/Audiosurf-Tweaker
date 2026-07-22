#include "ui/widgets/color_picker.hxx"

#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace
{
constexpr float k_rounding = 5.f;
constexpr float k_sv_default = 140.f;
constexpr float k_bar_h = 14.f;
constexpr float k_gap = 6.f;
constexpr float k_inputs_min_w = 100.f;
constexpr float k_sv_min = 64.f;
constexpr float k_row_gap_min = 4.f;
constexpr float k_label_w = 18.f;
constexpr float k_eps = 0.0005f;

namespace wdetail = tw::ui::widgets::detail;

bool near_equal(const ImVec4& a, const ImVec4& b) noexcept
{
    return std::fabs(a.x - b.x) < k_eps && std::fabs(a.y - b.y) < k_eps && std::fabs(a.z - b.z) < k_eps && std::fabs(a.w - b.w) < k_eps;
}

ImU32 to_u32_opaque(float r, float g, float b) noexcept
{
    return wdetail::to_u32(ImVec4 { r, g, b, 1.f });
}

bool parse_hex_color(std::string_view text, ImVec4& out) noexcept
{
    if(!text.empty() && text.front() == '#') {
        text.remove_prefix(1);
    }

    if(text.size() != 6 && text.size() != 8) {
        return false;
    }

    for(char c : text) {
        if(!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    std::uint32_t value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    if(std::from_chars(begin, end, value, 16).ec != std::errc {}) {
        return false;
    }

    if(text.size() == 6) {
        value |= 0xFF000000u;
    }

    out = ImVec4 {
        static_cast<float>((value >> 16) & 0xFFu) / 255.f,
        static_cast<float>((value >> 8) & 0xFFu) / 255.f,
        static_cast<float>(value & 0xFFu) / 255.f,
        static_cast<float>((value >> 24) & 0xFFu) / 255.f,
    };
    return true;
}

void format_hex(char* buf, std::size_t buf_size, const ImVec4& rgba) noexcept
{
    const auto ch = [](float f) -> int {
        return std::clamp(static_cast<int>(f * 255.f + 0.5f), 0, 255);
    };
    std::snprintf(buf, buf_size, "#%02X%02X%02X%02X", ch(rgba.w), ch(rgba.x), ch(rgba.y), ch(rgba.z));
}

// Re-color vertices [vtx_begin, VtxBuffer.Size) by projecting them onto the p0 -> p1 axis.
// Existing vertex alpha is used as a multiplier so the anti-aliasing fringe of a rounded fill
// (alpha == 0 verts) survives — that is what keeps corners round instead of square.
void shade_verts_gradient(ImDrawList* draw, int vtx_begin, const ImVec2& p0, const ImVec2& p1, ImU32 col0, ImU32 col1) noexcept
{
    const int vtx_end = draw->VtxBuffer.Size;
    if(vtx_end <= vtx_begin) {
        return;
    }

    const ImVec2 axis { p1.x - p0.x, p1.y - p0.y };
    const float len_sq = axis.x * axis.x + axis.y * axis.y;
    const float inv_len_sq = len_sq > 0.f ? 1.f / len_sq : 0.f;

    const auto channel = [](ImU32 col, int shift) -> float {
        return static_cast<float>((col >> shift) & 0xFFu);
    };
    const float r0 = channel(col0, IM_COL32_R_SHIFT);
    const float g0 = channel(col0, IM_COL32_G_SHIFT);
    const float b0 = channel(col0, IM_COL32_B_SHIFT);
    const float a0 = channel(col0, IM_COL32_A_SHIFT);
    const float dr = channel(col1, IM_COL32_R_SHIFT) - r0;
    const float dg = channel(col1, IM_COL32_G_SHIFT) - g0;
    const float db = channel(col1, IM_COL32_B_SHIFT) - b0;
    const float da = channel(col1, IM_COL32_A_SHIFT) - a0;

    ImDrawVert* const begin = draw->VtxBuffer.Data + vtx_begin;
    ImDrawVert* const end = draw->VtxBuffer.Data + vtx_end;
    for(ImDrawVert* vtx = begin; vtx < end; ++vtx) {
        const float t = std::clamp(((vtx->pos.x - p0.x) * axis.x + (vtx->pos.y - p0.y) * axis.y) * inv_len_sq, 0.f, 1.f);
        const float keep = channel(vtx->col, IM_COL32_A_SHIFT) / 255.f;
        vtx->col = IM_COL32(
            static_cast<int>(r0 + dr * t + 0.5f),
            static_cast<int>(g0 + dg * t + 0.5f),
            static_cast<int>(b0 + db * t + 0.5f),
            static_cast<int>((a0 + da * t) * keep + 0.5f));
    }
}

void add_rect_gradient(
    ImDrawList* draw, const ImVec2& p_min, const ImVec2& p_max, ImU32 col0, ImU32 col1, bool vertical, float rounding) noexcept
{
    const int vtx_begin = draw->VtxBuffer.Size;
    draw->AddRectFilled(p_min, p_max, IM_COL32_WHITE, rounding);
    const ImVec2 axis_end = vertical ? ImVec2 { p_min.x, p_max.y } : ImVec2 { p_max.x, p_min.y };
    shade_verts_gradient(draw, vtx_begin, p_min, axis_end, col0, col1);
}

// Six linear stops. Geometry is cut per stop rather than scissored out of one big shape: vertex
// colours are clamped to the gradient ends, so a vertex sitting outside its own stop would flatten
// the whole ramp. Only the outer caps are rounded.
void add_hue_gradient(ImDrawList* draw, const ImRect& bb, float rounding) noexcept
{
    static const ImU32 k_hues[7] = {
        IM_COL32(255, 0, 0, 255),
        IM_COL32(255, 255, 0, 255),
        IM_COL32(0, 255, 0, 255),
        IM_COL32(0, 255, 255, 255),
        IM_COL32(0, 0, 255, 255),
        IM_COL32(255, 0, 255, 255),
        IM_COL32(255, 0, 0, 255),
    };

    const float seg_w = (bb.Max.x - bb.Min.x) / 6.f;
    const auto stop_x = [&](int i) -> float {
        return i == 6 ? bb.Max.x : bb.Min.x + seg_w * static_cast<float>(i);
    };

    const auto stop = [&](int i, float x_left, float x_right, ImDrawFlags flags) {
        const int vtx_begin = draw->VtxBuffer.Size;
        draw->AddRectFilled(ImVec2 { x_left, bb.Min.y }, ImVec2 { x_right, bb.Max.y }, IM_COL32_WHITE, rounding, flags);
        shade_verts_gradient(
            draw,
            vtx_begin,
            ImVec2 { stop_x(i), bb.Min.y },
            ImVec2 { stop_x(i + 1), bb.Min.y },
            wdetail::apply_style_alpha(k_hues[i]),
            wdetail::apply_style_alpha(k_hues[i + 1]));
    };

    // Caps bleed a pixel into their neighbour: the flat middle overdraws the first cap's
    // anti-aliased inner edge, and the last cap's fringe lands on a near-identical colour.
    constexpr float k_bleed = 1.f;
    stop(0, bb.Min.x, stop_x(1) + k_bleed, ImDrawFlags_RoundCornersLeft);
    for(int i = 1; i < 5; ++i) {
        stop(i, stop_x(i), stop_x(i + 1), ImDrawFlags_RoundCornersNone);
    }
    stop(5, stop_x(5) - k_bleed, bb.Max.x, ImDrawFlags_RoundCornersRight);
}

void draw_bar_marker(ImDrawList* draw, const ImRect& bb, float t) noexcept
{
    constexpr float k_inset = 2.5f;
    const float x = std::clamp(bb.Min.x + t * (bb.Max.x - bb.Min.x), bb.Min.x + k_inset, bb.Max.x - k_inset);
    const float y0 = bb.Min.y + 1.f;
    const float y1 = bb.Max.y - 1.f;

    const ImU32 black = wdetail::to_u32(ImVec4 { 0.f, 0.f, 0.f, 0.85f });
    const ImU32 white = wdetail::to_u32(ImVec4 { 1.f, 1.f, 1.f, 0.95f });
    draw->AddLine(ImVec2 { x, y0 }, ImVec2 { x, y1 }, black, 3.f);
    draw->AddLine(ImVec2 { x, y0 }, ImVec2 { x, y1 }, white, 1.5f);
}

bool drag_sv(const ImRect& bb, ImGuiID id, float& s, float& v)
{
    bool hovered = false;
    bool held = false;
    ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if(!held) {
        return false;
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float w = std::max(bb.Max.x - bb.Min.x, 1.f);
    const float h = std::max(bb.Max.y - bb.Min.y, 1.f);
    s = std::clamp((mouse.x - bb.Min.x) / w, 0.f, 1.f);
    v = std::clamp(1.f - (mouse.y - bb.Min.y) / h, 0.f, 1.f);
    return true;
}

bool drag_bar(const ImRect& bb, ImGuiID id, float& t)
{
    bool hovered = false;
    bool held = false;
    ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if(!held) {
        return false;
    }

    const float w = std::max(bb.Max.x - bb.Min.x, 1.f);
    t = std::clamp((ImGui::GetIO().MousePos.x - bb.Min.x) / w, 0.f, 1.f);
    return true;
}
} // namespace

namespace tw::ui::widgets
{
color_picker::color_picker(const char* id, ImVec2 size) : m_id(id ? id : "color_picker"), m_size(size)
{
    rebuild_rgba_from_hsv();
    sync_fields_from_rgba();
}

void color_picker::set_size(ImVec2 size) noexcept
{
    m_size = size;
}

void color_picker::set_color(const ImVec4& rgba) noexcept
{
    m_rgba = rgba;
    m_a = std::clamp(rgba.w, 0.f, 1.f);

    float h = m_h;
    float s = 0.f;
    float v = 0.f;
    ImGui::ColorConvertRGBtoHSV(rgba.x, rgba.y, rgba.z, h, s, v);
    if(s > 0.001f) {
        m_h = h;
    }
    m_s = std::clamp(s, 0.f, 1.f);
    m_v = std::clamp(v, 0.f, 1.f);
    sync_fields_from_rgba();
}

void color_picker::rebuild_rgba_from_hsv() noexcept
{
    ImGui::ColorConvertHSVtoRGB(m_h, m_s, m_v, m_rgba.x, m_rgba.y, m_rgba.z);
    m_rgba.w = m_a;
}

void color_picker::sync_fields_from_rgba() noexcept
{
    format_hex(m_hex_buf, sizeof(m_hex_buf), m_rgba);
    m_rgba_i[0] = std::clamp(static_cast<int>(m_rgba.x * 255.f + 0.5f), 0, 255);
    m_rgba_i[1] = std::clamp(static_cast<int>(m_rgba.y * 255.f + 0.5f), 0, 255);
    m_rgba_i[2] = std::clamp(static_cast<int>(m_rgba.z * 255.f + 0.5f), 0, 255);
    m_rgba_i[3] = std::clamp(static_cast<int>(m_rgba.w * 255.f + 0.5f), 0, 255);
}

void color_picker::apply_rgba_ints() noexcept
{
    for(int& c : m_rgba_i) {
        c = std::clamp(c, 0, 255);
    }

    const ImVec4 rgba {
        static_cast<float>(m_rgba_i[0]) / 255.f,
        static_cast<float>(m_rgba_i[1]) / 255.f,
        static_cast<float>(m_rgba_i[2]) / 255.f,
        static_cast<float>(m_rgba_i[3]) / 255.f,
    };
    set_color(rgba);
}

bool color_picker::try_apply_hex() noexcept
{
    ImVec4 parsed {};
    if(!parse_hex_color(m_hex_buf, parsed)) {
        sync_fields_from_rgba();
        return false;
    }
    set_color(parsed);
    return true;
}

void color_picker::update()
{
    update_internal();
}

void color_picker::update(ImVec4& color)
{
    if(!near_equal(color, m_rgba)) {
        set_color(color);
    }
    update_internal();
    color = m_rgba;
}

void color_picker::update_internal()
{
    m_changed = false;

    ImGui::PushID(m_id.c_str());
    ImGui::BeginGroup();

    float widget_w = m_size.x;
    if(widget_w <= 0.f) {
        widget_w = ImGui::GetContentRegionAvail().x;
    }
    if(widget_w <= 0.f) {
        widget_w = k_sv_default + k_gap + k_inputs_min_w;
    }

    // The inputs column (hex + 4 channel rows) sits next to the SV square: the square must not be
    // shorter than that column, otherwise the rows spill over the hue/alpha bars below.
    const float line_h = ImGui::GetFrameHeight();
    const float inputs_h = 5.f * line_h + 4.f * k_row_gap_min;

    float sv_size = k_sv_default;
    if(m_size.y > 0.f) {
        sv_size = m_size.y - 2.f * k_bar_h - 2.f * k_gap;
    }
    sv_size = std::max({ sv_size, k_sv_min, inputs_h });

    float inputs_w = widget_w - sv_size - k_gap;
    if(inputs_w < k_inputs_min_w) {
        inputs_w = k_inputs_min_w;
        sv_size = std::max(widget_w - k_gap - inputs_w, k_sv_min);
        widget_w = sv_size + k_gap + inputs_w;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    // --- SV square ---
    const ImRect sv_bb { origin, ImVec2 { origin.x + sv_size, origin.y + sv_size } };
    ImGui::ItemSize(sv_bb.GetSize());
    const ImGuiID sv_id = ImGui::GetID("##sv");
    if(ImGui::ItemAdd(sv_bb, sv_id)) {
        if(drag_sv(sv_bb, sv_id, m_s, m_v)) {
            m_changed = true;
            rebuild_rgba_from_hsv();
        }

        float hue_r = 1.f;
        float hue_g = 0.f;
        float hue_b = 0.f;
        ImGui::ColorConvertHSVtoRGB(m_h, 1.f, 1.f, hue_r, hue_g, hue_b);
        const ImU32 col_white = to_u32_opaque(1.f, 1.f, 1.f);
        const ImU32 col_hue = to_u32_opaque(hue_r, hue_g, hue_b);
        const ImU32 col_black = to_u32_opaque(0.f, 0.f, 0.f);
        const ImU32 col_black_clear = wdetail::to_u32(ImVec4 { 0.f, 0.f, 0.f, 0.f });

        // Two rounded layers instead of one clipped multi-colour quad: saturation left -> right,
        // then value as a black veil top -> bottom.
        add_rect_gradient(draw, sv_bb.Min, sv_bb.Max, col_white, col_hue, false, k_rounding);
        add_rect_gradient(draw, sv_bb.Min, sv_bb.Max, col_black_clear, col_black, true, k_rounding);
        draw->AddRect(sv_bb.Min, sv_bb.Max, wdetail::to_u32(theme::border), k_rounding);

        const float cx = sv_bb.Min.x + m_s * sv_size;
        const float cy = sv_bb.Min.y + (1.f - m_v) * sv_size;
        const ImU32 cursor_fill = to_u32_opaque(m_rgba.x, m_rgba.y, m_rgba.z);
        draw->AddCircleFilled(ImVec2 { cx, cy }, 6.f, cursor_fill, 16);
        draw->AddCircle(ImVec2 { cx, cy }, 6.f, wdetail::to_u32(ImVec4 { 0.f, 0.f, 0.f, 0.65f }), 16, 2.f);
        draw->AddCircle(ImVec2 { cx, cy }, 6.f, wdetail::to_u32(ImVec4 { 1.f, 1.f, 1.f, 0.95f }), 16, 1.25f);
    }

    // --- Inputs column ---
    const ImGuiID hex_id = ImGui::GetID("##hex");
    const ImGuiID r_id = ImGui::GetID("##r");
    const ImGuiID g_id = ImGui::GetID("##g");
    const ImGuiID b_id = ImGui::GetID("##b");
    const ImGuiID a_id = ImGui::GetID("##a");
    const ImGuiID active = ImGui::GetActiveID();
    const bool editing_fields = active == hex_id || active == r_id || active == g_id || active == b_id || active == a_id;
    if(!editing_fields) {
        sync_fields_from_rgba();
    }

    ImGui::SameLine(0.f, k_gap);
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_FrameBg, theme::surface_input);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, theme::surface_hover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, theme::surface_elevated);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::border);

    const float field_w = inputs_w;
    const float row_gap = std::max(k_row_gap_min, (sv_size - 5.f * line_h) / 4.f);

    // Rows are spaced by ItemSpacing only — explicit Dummy() spacers would add their own spacing
    // on top and push the last channel row past the bottom of the SV square.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2 { ImGui::GetStyle().ItemSpacing.x, row_gap });

    auto draw_channel_row = [&](const char* label, const char* id, int* value) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(k_label_w);
        ImGui::SetNextItemWidth(field_w - k_label_w);
        if(ImGui::InputScalar(id, ImGuiDataType_S32, value, nullptr, nullptr, "%d")) {
            apply_rgba_ints();
            m_changed = true;
        }
        if(ImGui::IsItemDeactivatedAfterEdit()) {
            apply_rgba_ints();
            m_changed = true;
        }
    };

    ImGui::SetNextItemWidth(field_w);
    ImGui::InputText("##hex", m_hex_buf, sizeof(m_hex_buf));
    if(ImGui::IsItemDeactivatedAfterEdit()) {
        if(try_apply_hex()) {
            m_changed = true;
        }
    }

    draw_channel_row("R", "##r", &m_rgba_i[0]);
    draw_channel_row("G", "##g", &m_rgba_i[1]);
    draw_channel_row("B", "##b", &m_rgba_i[2]);
    draw_channel_row("A", "##a", &m_rgba_i[3]);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
    ImGui::EndGroup();

    ImGui::SetCursorScreenPos(ImVec2 { origin.x, origin.y + sv_size + k_gap });

    // --- Hue bar (full widget width) ---
    const ImRect hue_bb { ImGui::GetCursorScreenPos(), ImVec2 { origin.x + widget_w, ImGui::GetCursorScreenPos().y + k_bar_h } };
    ImGui::ItemSize(hue_bb.GetSize());
    const ImGuiID hue_id = ImGui::GetID("##hue");
    if(ImGui::ItemAdd(hue_bb, hue_id)) {
        if(drag_bar(hue_bb, hue_id, m_h)) {
            m_changed = true;
            rebuild_rgba_from_hsv();
        }

        add_hue_gradient(draw, hue_bb, k_rounding);
        draw->AddRect(hue_bb.Min, hue_bb.Max, wdetail::to_u32(theme::border), k_rounding);
        draw_bar_marker(draw, hue_bb, m_h);
    }

    ImGui::SetCursorScreenPos(ImVec2 { origin.x, hue_bb.Max.y + k_gap });

    // --- Alpha bar (full widget width) ---
    const ImRect alpha_bb { ImGui::GetCursorScreenPos(), ImVec2 { origin.x + widget_w, ImGui::GetCursorScreenPos().y + k_bar_h } };
    ImGui::ItemSize(alpha_bb.GetSize());
    const ImGuiID alpha_id = ImGui::GetID("##alpha");
    if(ImGui::ItemAdd(alpha_bb, alpha_id)) {
        if(drag_bar(alpha_bb, alpha_id, m_a)) {
            m_changed = true;
            rebuild_rgba_from_hsv();
        }

        const ImU32 opaque = wdetail::to_u32(ImVec4 { m_rgba.x, m_rgba.y, m_rgba.z, 1.f });
        const ImU32 clear = wdetail::to_u32(ImVec4 { m_rgba.x, m_rgba.y, m_rgba.z, 0.f });
        ImGui::RenderColorRectWithAlphaCheckerboard(
            draw, alpha_bb.Min, alpha_bb.Max, 0, k_bar_h * 0.5f, ImVec2 { 0.f, 0.f }, k_rounding);
        // Alpha grows left -> right, matching the marker position.
        add_rect_gradient(draw, alpha_bb.Min, alpha_bb.Max, clear, opaque, false, k_rounding);
        draw->AddRect(alpha_bb.Min, alpha_bb.Max, wdetail::to_u32(theme::border), k_rounding);
        draw_bar_marker(draw, alpha_bb, m_a);
    }

    ImGui::SetCursorScreenPos(ImVec2 { origin.x, alpha_bb.Max.y });
    ImGui::Dummy(ImVec2 { widget_w, 0.f });

    ImGui::EndGroup();
    ImGui::PopID();
}

ImVec4 color_picker::color() const noexcept
{
    return m_rgba;
}

bool color_picker::changed() const noexcept
{
    return m_changed;
}
} // namespace tw::ui::widgets
