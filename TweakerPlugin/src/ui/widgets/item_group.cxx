#include "ui/widgets/item_group.hxx"

#include "ui/kit/paint.hxx"
#include "ui/theme.hxx"

#include <imgui_internal.h>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace
{
constexpr float k_title_gap_pad = 4.f;
constexpr float k_min_title_inset = 8.f;

ImVec2 snap_px(ImVec2 p) noexcept
{
    return ImVec2 { std::floor(p.x), std::floor(p.y) };
}

// The frame with a gap in its top edge under the title. The same AddRect as every other border (a custom
// PathStroke's anti-aliasing looked uneven), drawn three times under three clip rects that leave the gap
// out: left of it, right of it, and below the top edge. Not a patch of background painted over the edge:
// whatever colour it guessed, it showed during a fade and on anything but a flat background.
void add_group_border(ImDrawList* draw, ImVec2 p_min, ImVec2 p_max, float rounding, float gap_x0, float gap_x1, ImU32 border_col) noexcept
{
    if(draw == nullptr) {
        return;
    }

    p_min = snap_px(p_min);
    p_max = snap_px(p_max);
    if(p_max.x <= p_min.x || p_max.y <= p_min.y) {
        return;
    }

    rounding = std::clamp(rounding, 0.f, std::min(p_max.x - p_min.x, p_max.y - p_min.y) * 0.5f);

    gap_x0 = std::floor(gap_x0);
    gap_x1 = std::floor(gap_x1);
    if(gap_x1 <= gap_x0) {
        draw->AddRect(p_min, p_max, border_col, rounding);
        return;
    }

    // The stroke is a pixel wide and anti-aliased a pixel either side of it, so "below the top edge" starts
    // two pixels down and the side regions reach a pixel past the frame.
    const ImVec2 outer_min { p_min.x - 1.f, p_min.y - 1.f };
    const ImVec2 outer_max { p_max.x + 1.f, p_max.y + 1.f };
    const ImVec4 regions[] = {
        { outer_min.x, outer_min.y, gap_x0, outer_max.y },
        { gap_x1, outer_min.y, outer_max.x, outer_max.y },
        { gap_x0, p_min.y + 2.f, gap_x1, outer_max.y },
    };
    for(const ImVec4& r : regions) {
        draw->PushClipRect(ImVec2 { r.x, r.y }, ImVec2 { r.z, r.w }, true);
        draw->AddRect(p_min, p_max, border_col, rounding);
        draw->PopClipRect();
    }
}
} // namespace

namespace tw::ui::widgets
{
item_group::item_group(const char* id, const char* group_title)
    : m_id(id ? id : "item_group"), m_group_title(group_title ? group_title : "")
{
}

void item_group::set_group_title(std::string_view title)
{
    m_group_title.assign(title.begin(), title.end());
}

void item_group::set_rounding(float rounding) noexcept
{
    m_rounding = rounding;
}

void item_group::set_outer_padding(ImVec2 padding) noexcept
{
    m_outer_padding = padding;
}

void item_group::set_inner_padding(ImVec2 padding) noexcept
{
    m_inner_padding = padding;
}

void item_group::begin()
{
    assert(!m_active && "item_group::begin() called while already active");
    m_active = true;

    ImGui::PushID(m_id.c_str());

    const ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2 { cursor.x + m_outer_padding.x, cursor.y + m_outer_padding.y });
    m_origin = ImGui::GetCursorScreenPos();

    m_title_h = 0.f;
    if(!m_group_title.empty()) {
        m_title_h = ImGui::CalcTextSize(m_group_title.c_str()).y;
    }

    const float top_inset = (m_title_h > 0.f ? m_title_h * 0.5f : 0.f) + m_inner_padding.y;
    ImGui::SetCursorScreenPos(ImVec2 { m_origin.x + m_inner_padding.x, m_origin.y + top_inset });

    // The content region ends where the frame's right padding starts, for as long as the group is open:
    // a widget that fills "the rest of the line" would otherwise fill it to the window's edge and take
    // the frame out past it. Internal state, saved and put back in end().
    ImGuiWindow* const window = ImGui::GetCurrentWindow();
    const float right = m_inner_padding.x + m_outer_padding.x;
    m_saved_content_max_x = window->ContentRegionRect.Max.x;
    m_saved_work_max_x = window->WorkRect.Max.x;
    window->ContentRegionRect.Max.x -= right;
    window->WorkRect.Max.x -= right;

    ImGui::BeginGroup();
}

void item_group::end()
{
    assert(m_active && "item_group::end() called without matching begin()");

    ImGui::EndGroup();

    ImGuiWindow* const window = ImGui::GetCurrentWindow();
    window->ContentRegionRect.Max.x = m_saved_content_max_x;
    window->WorkRect.Max.x = m_saved_work_max_x;

    ImVec2 content_min = ImGui::GetItemRectMin();
    ImVec2 content_max = ImGui::GetItemRectMax();
    if(content_max.x < content_min.x || content_max.y < content_min.y) {
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        content_min = cursor;
        content_max = cursor;
    }

    const float frame_top = m_origin.y + (m_title_h > 0.f ? m_title_h * 0.5f : 0.f);
    ImVec2 frame_min { m_origin.x, frame_top };
    ImVec2 frame_max { content_max.x + m_inner_padding.x, content_max.y + m_inner_padding.y };

    if(frame_max.x < frame_min.x + 1.f) {
        frame_max.x = frame_min.x + 1.f;
    }
    if(frame_max.y < frame_min.y + 1.f) {
        frame_max.y = frame_min.y + 1.f;
    }

    float gap_x0 = 0.f;
    float gap_x1 = 0.f;
    ImVec2 title_pos {};

    if(m_title_h > 0.f && !m_group_title.empty()) {
        const ImVec2 title_size = ImGui::CalcTextSize(m_group_title.c_str());
        const float title_inset = std::max(m_rounding, k_min_title_inset);
        title_pos = ImVec2 { frame_min.x + title_inset, frame_min.y - title_size.y * 0.5f };
        gap_x0 = title_pos.x - k_title_gap_pad;
        gap_x1 = title_pos.x + title_size.x + k_title_gap_pad;

        const float title_right = gap_x1 + std::max(m_rounding, k_min_title_inset);
        if(title_right > frame_max.x) {
            frame_max.x = title_right;
        }
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    add_group_border(draw, frame_min, frame_max, m_rounding, gap_x0, gap_x1, kit::paint::to_u32(theme::border));

    if(m_title_h > 0.f && !m_group_title.empty()) {
        title_pos = snap_px(title_pos);
        draw->AddText(title_pos, kit::paint::to_u32(theme::text_secondary), m_group_title.c_str());
    }

    // Claim uses pre-snap layout size so Dummy matches content measurement.
    const float claim_w = (frame_max.x - m_origin.x) + m_outer_padding.x;
    const float claim_h = (frame_max.y - m_origin.y) + m_outer_padding.y;
    ImGui::SetCursorScreenPos(m_origin);
    ImGui::Dummy(ImVec2 { std::max(claim_w, 1.f), std::max(claim_h, 1.f) });

    ImGui::PopID();
    m_active = false;
}
} // namespace tw::ui::widgets
