// No TweakerPlugin PCH here - part of tweaker_ui, shared with smoke_test (same as ui/widgets/).
#include "ui/kit/interact.hxx"

namespace tw::ui::kit
{
interaction interact(const ImRect& bb, ImGuiID id, placement where, bool enabled)
{
    interaction result;

    if(where == placement::flow) {
        ImGui::ItemSize(bb);
    }

    // The item flag rather than BeginDisabled(): BeginDisabled also multiplies the style alpha, and how a
    // disabled element looks is the drawing's decision, not this function's.
    if(!enabled) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    }

    if(ImGui::ItemAdd(bb, id)) {
        result.visible = true;
        result.disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
        result.clicked = ImGui::ButtonBehavior(bb, id, &result.hovered, &result.held);

        if(result.hovered) {
            result.left_down = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            result.right_down = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
            result.double_clicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        }
    }

    if(!enabled) {
        ImGui::PopItemFlag();
    }

    return result;
}

bool fade_disabled(bool enabled) noexcept
{
    return !enabled && (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) == 0;
}

bool hovering(const ImRect& bb)
{
    // AllowWhenBlockedByActiveItem: a row stays lit while one of its own controls is held, or while it is
    // being dragged by its grip - the active item is ours, not something covering the row.
    return ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(bb.Min, bb.Max);
}
} // namespace tw::ui::kit
