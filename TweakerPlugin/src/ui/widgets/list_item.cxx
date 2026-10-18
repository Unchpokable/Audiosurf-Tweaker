#include "ui/widgets/list_item.hxx"

#include "ui/kit/interact.hxx"
#include "ui/kit/paint.hxx"
#include "ui/theme.hxx"

#include <imgui_internal.h>

namespace
{
constexpr float k_pad = 8.f;
constexpr float k_icon_gap = 8.f;

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

    const ImVec2 size = kit::paint::resolve_size(m_size, 200.f, 36.f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb { pos, ImVec2 { pos.x + size.x, pos.y + size.y } };

    const kit::interaction pointer = kit::interact(bb, ImGui::GetID("##row"));
    m_hovered = pointer.hovered;
    m_held = pointer.held;
    m_clicked = pointer.clicked;
    if(!pointer.visible) {
        ImGui::PopID();
        return;
    }

    m_hover.set(m_hovered);
    m_select.set(m_selected);
    const float hover_t = m_hover.step();
    const float select_t = m_select.step();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    kit::paint::row_background(draw, bb.Min, bb.Max, hover_t, select_t);
    draw_content(draw, bb.Min, bb.Max, content);

    ImGui::PopID();
}

void list_item::draw_content(ImDrawList* draw, const ImVec2& min, const ImVec2& max, const list_item_content& content)
{
    const float row_h = max.y - min.y;

    float text_x = min.x + k_pad;
    const bool has_icon = content.icon != ImTextureID_Invalid;
    if(has_icon) {
        const float icon_slot = row_h - k_pad * 2.f;
        const ImVec2 slot_min { min.x + k_pad, min.y + k_pad };
        const ImVec2 slot_max { slot_min.x + icon_slot, slot_min.y + icon_slot };
        ImVec2 src = content.icon_size;
        if(src.x <= 0.f || src.y <= 0.f) {
            src = ImVec2 { icon_slot, icon_slot };
        }
        kit::paint::add_image_keep_aspect(draw, content.icon, src, slot_min, slot_max);
        text_x = slot_max.x + k_icon_gap;
    }

    // Ellipsized rather than clipped at the row's edge: a list of long names (playlists, most of the
    // time) otherwise ends in a hard vertical cut mid-glyph with no sign that anything was lost.
    const float text_max_x = max.x - k_pad;

    if(!content.text.empty() && content.subtext.empty()) {
        const ImVec2 text_size = ImGui::CalcTextSize(content.text.c_str());
        const ImVec2 text_pos { text_x, min.y + (row_h - text_size.y) * 0.5f };
        kit::paint::add_text_ellipsis(draw, text_pos, text_max_x, kit::paint::to_u32(theme::text_primary), content.text.c_str());
    }
    else if(!content.text.empty()) {
        // Two lines, centred as a block: primary at the window font size, secondary smaller and
        // muted, the same relationship the desktop's playlist rows use for name and track count.
        const float primary_h = ImGui::GetFontSize();
        const float block_h = primary_h + k_subtext_gap + k_subtext_size;
        const float top = min.y + (row_h - block_h) * 0.5f;

        kit::paint::add_text_ellipsis(
            draw, ImVec2 { text_x, top }, text_max_x, kit::paint::to_u32(theme::text_primary), content.text.c_str());

        // The second line is cut the same way, at its own size (R6): measured with the font pushed at
        // that size, so the ellipsis lands where the smaller glyphs actually end.
        ImGui::PushFont(nullptr, k_subtext_size);
        kit::paint::add_text_ellipsis(draw,
            ImVec2 { text_x, top + primary_h + k_subtext_gap },
            text_max_x,
            kit::paint::to_u32(theme::text_muted),
            content.subtext.c_str());
        ImGui::PopFont();
    }
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
