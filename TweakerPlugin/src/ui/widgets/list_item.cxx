#include "ui/widgets/list_item.hxx"

#include "ui/theme.hxx"
#include "ui/widgets/detail/draw.hxx"

#include <imgui_internal.h>

namespace
{
constexpr float k_rounding = 8.f;
constexpr float k_pad = 8.f;
constexpr float k_icon_gap = 8.f;
constexpr int k_anim_ms = 150;

// Secondary line: noticeably smaller than the primary without dropping below what the baked atlas
// resolves cleanly at.
constexpr float k_subtext_size = 12.f;
constexpr float k_subtext_gap = 2.f;
} // namespace

namespace tw::ui::widgets
{
list_item::list_item(const char* id, ImVec2 size) : m_id(id ? id : "list_item"), m_size(size)
{
}

void list_item::set_size(ImVec2 size) noexcept
{
    m_size = size;
}

void list_item::set_content(list_item_content content)
{
    m_content = std::move(content);
}

void list_item::set_selected(bool selected) noexcept
{
    m_selected = selected;
}

void list_item::retarget_hover(float target)
{
    m_hover_tween = tweeny::from(m_hover_t).to(target).during(k_anim_ms).via(tweeny::easing::cubicOut);
}

void list_item::retarget_select(float target)
{
    m_select_tween = tweeny::from(m_select_t).to(target).during(k_anim_ms).via(tweeny::easing::cubicOut);
}

void list_item::update()
{
    draw_frame(m_content);
}

void list_item::update(const list_item_content& content)
{
    draw_frame(content);
}

void list_item::draw_frame(const list_item_content& content)
{
    m_clicked = false;

    ImGui::PushID(m_id.c_str());

    const ImVec2 size = detail::resolve_size(m_size, 200.f, 36.f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb { pos, ImVec2 { pos.x + size.x, pos.y + size.y } };

    ImGui::ItemSize(bb);
    const ImGuiID imgui_id = ImGui::GetID("##row");
    if(!ImGui::ItemAdd(bb, imgui_id)) {
        ImGui::PopID();
        return;
    }

    m_hovered = false;
    m_held = false;
    m_clicked = ImGui::ButtonBehavior(bb, imgui_id, &m_hovered, &m_held);

    if(m_hovered != m_was_hovered) {
        retarget_hover(m_hovered ? 1.f : 0.f);
    }
    if(m_selected != m_was_selected) {
        retarget_select(m_selected ? 1.f : 0.f);
    }

    m_was_hovered = m_hovered;
    m_was_selected = m_selected;

    // Own floats are source of truth — do not peek() default/from-only tweens (current is unset).
    const auto dt = detail::dt_ms();
    if(!m_hover_tween.isFinished()) {
        m_hover_t = m_hover_tween.step(dt);
    }
    if(!m_select_tween.isFinished()) {
        m_select_t = m_select_tween.step(dt);
    }

    ImVec4 fill = detail::lerp(theme::surface_row, theme::surface_row_hover, m_hover_t);
    fill = detail::lerp(fill, theme::accent_selected, m_select_t);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    detail::add_rect_filled_rounded(draw, bb.Min, bb.Max, detail::to_u32(fill), k_rounding);
    if(m_select_t > 0.01f) {
        ImVec4 border_col = detail::lerp(theme::border, theme::accent_border, m_select_t);
        draw->AddRect(bb.Min, bb.Max, detail::to_u32(border_col), k_rounding);
    }

    float text_x = bb.Min.x + k_pad;
    const bool has_icon = content.icon != ImTextureID_Invalid;
    if(has_icon) {
        const float icon_slot = size.y - k_pad * 2.f;
        const ImVec2 slot_min { bb.Min.x + k_pad, bb.Min.y + k_pad };
        const ImVec2 slot_max { slot_min.x + icon_slot, slot_min.y + icon_slot };
        ImVec2 src = content.icon_size;
        if(src.x <= 0.f || src.y <= 0.f) {
            src = ImVec2 { icon_slot, icon_slot };
        }
        detail::add_image_keep_aspect(draw, content.icon, src, slot_min, slot_max);
        text_x = slot_max.x + k_icon_gap;
    }

    // Ellipsized rather than clipped at the row's edge: a list of long names (playlists, most of the
    // time) otherwise ends in a hard vertical cut mid-glyph with no sign that anything was lost.
    const float text_max_x = bb.Max.x - k_pad;

    if(!content.text.empty() && content.subtext.empty()) {
        const ImVec2 text_size = ImGui::CalcTextSize(content.text.c_str());
        const ImVec2 text_pos { text_x, bb.Min.y + (size.y - text_size.y) * 0.5f };
        detail::add_text_ellipsis(draw, text_pos, text_max_x, detail::to_u32(theme::text_primary), content.text.c_str());
    }
    else if(!content.text.empty()) {
        // Two lines, centred as a block: primary at the window font size, secondary smaller and
        // muted, the same relationship the desktop's playlist rows use for name and track count.
        const float primary_h = ImGui::GetFontSize();
        const float block_h = primary_h + k_subtext_gap + k_subtext_size;
        const float top = bb.Min.y + (size.y - block_h) * 0.5f;

        detail::add_text_ellipsis(draw, ImVec2 { text_x, top }, text_max_x, detail::to_u32(theme::text_primary), content.text.c_str());
        detail::add_text_scaled(draw,
            k_subtext_size,
            ImVec2 { text_x, top + primary_h + k_subtext_gap },
            detail::to_u32(theme::text_muted),
            content.subtext.c_str());
    }

    ImGui::PopID();
}

bool list_item::selected() const noexcept
{
    return m_selected;
}

bool list_item::clicked() const noexcept
{
    return m_clicked;
}

bool list_item::hovered() const noexcept
{
    return m_hovered;
}
} // namespace tw::ui::widgets
