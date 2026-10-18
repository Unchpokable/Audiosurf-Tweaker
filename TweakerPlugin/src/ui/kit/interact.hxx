#pragma once

#include <imgui.h>
#include <imgui_internal.h>

// One answer to "what is the pointer doing to this element" - the layer under every interactive thing
// in the overlay, widget or hand-drawn. Docs/Internal/tweaker-plugin-widgets.md, "kit::interact".
//
// Before this there were three sources of hover and they did not agree. ButtonBehavior and
// InvisibleButton respect whatever ImGui knows about the frame: a window on top, a page fading out with
// NoInputs, an item already holding the mouse. IsMouseHoveringRect knows none of that - it is plain
// geometry - and the hand-drawn places that used it lit up under the sky panel docked over the menu,
// under tooltips, and on a tab page that was on its way out.
namespace tw::ui::kit
{
// Whether the element takes space in the layout or only a hit area at a place computed by hand (a tab
// in tab_view's nav column, a chip inside a segmented track, the menu's title bar) whose space is
// claimed some other way.
enum class placement {
    flow,
    overlay,
};

// What the pointer did to one element this frame.
struct interaction {
    // Laid out and not clipped away. When false every field below is false: the element is not on screen,
    // so nothing can be happening to it - and a caller that keeps a hovered() answer for later must not
    // keep the one from the last frame it was visible.
    bool visible = false;

    // Disabled through `enabled` or an enclosing ImGui::BeginDisabled(). A disabled element is neither
    // hovered nor pressed; drawing it differently is the caller's part.
    bool disabled = false;

    bool hovered = false;

    // The left button went down on it and is still down - wherever the pointer is now.
    bool held = false;

    // A full left click: down on it, released on it. What a button means by "pressed".
    bool clicked = false;

    // The left / right button went down over it this frame - for actions that should not wait for the
    // release, like opening a popup where the click happened.
    bool left_down = false;
    bool right_down = false;

    // The second click of a left double click, over it.
    bool double_clicked = false;
};

[[nodiscard]] interaction interact(const ImRect& bb, ImGuiID id, placement where = placement::flow, bool enabled = true);

// Hover for a region that has other elements on it - a hand-drawn row carrying its own toggle and
// buttons - and so must not be an element itself: ImGui gives hover to the first element that claims
// it, and the row would take it from its own controls. This answers the question without claiming
// anything: the pointer is inside `bb`, inside the current clip rect, and the current window is the one
// it is over. Not for anything clickable - that is interact().
[[nodiscard]] bool hovering(const ImRect& bb);

// Whether a control disabled by its own flag (set_enabled(false)) should fade itself by
// theme::disabled_alpha: not inside ImGui's BeginDisabled(), which has already faded everything in it by
// Style.DisabledAlpha - fading again would square it.
[[nodiscard]] bool fade_disabled(bool enabled) noexcept;
} // namespace tw::ui::kit
