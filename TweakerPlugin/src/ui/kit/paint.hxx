#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <string>
#include <string_view>

// Drawing recipes without state: what a widget and a hand-drawn part of the overlay both use to look the
// same. Docs/Internal/tweaker-plugin-widgets.md, "kit::paint".
//
// Colours. An ImVec4 parameter is a theme colour as the theme holds it, and the recipe applies the style
// alpha (tab pages crossfading, a popup fading) to it once. An ImU32 parameter is a finished colour, already
// through to_u32(), and is drawn as given. Applying the alpha a second time is the bug this rule exists for:
// an element fades as alpha squared and lags behind everything around it during a transition.
namespace tw::ui::kit::paint
{
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

// A theme colour to a finished one: the style alpha applied, as ImGui::GetColorU32 does.
inline ImU32 to_u32(const ImVec4& c) noexcept
{
    ImVec4 m = c;
    m.w *= ImGui::GetStyle().Alpha;
    return ImGui::ColorConvertFloat4ToU32(m);
}

// The same for a colour that is an ImU32 constant to begin with (color_picker's hue stops).
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

// A size of 0 on an axis means "the default": the rest of the line for the width, `default_h` for the height.
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

// Fit `src` into `slot` preserving aspect ratio (letterbox / pillarbox).
ImVec2 aspect_fit_size(ImVec2 src, ImVec2 slot) noexcept;

// A picture fitted into `slot` with its aspect kept and centred - a skin preview, the watermark logo.
// A `src_size` of 0 means "the slot's own shape". `tint` multiplies the texture.
void add_image_keep_aspect(ImDrawList* draw,
    ImTextureID tex,
    ImVec2 src_size,
    const ImVec2& slot_min,
    const ImVec2& slot_max,
    const ImVec4& tint = ImVec4 { 1.f, 1.f, 1.f, 1.f }) noexcept;

// A monochrome icon from ui/image/svg, `px` pixels square, tinted with `tint` (the assets are white, so the
// tint is the icon's colour). Pass the size it was baked at - get_resource(key).at(px) - and the texture lands
// texel for texel: `min` is snapped down to the pixel grid, as ImGui snaps text, because at a half-pixel
// position the linear filter spreads every edge over two pixels. ImTextureID_Invalid draws nothing.
void icon(ImDrawList* draw, ImTextureID tex, ImVec2 min, float px, const ImVec4& tint) noexcept;

// The background of a list row. A row is a card (overlay-widgets-history.md §2, decision 3): its own fill, theme::radius::row,
// and theme::space::row_gap to the next one - the gap is the caller's layout, not drawn here. `hover_t` and
// `select_t` in [0, 1]; a selected row does not change further under the pointer.
void row_background(ImDrawList* draw, const ImVec2& min, const ImVec2& max, float hover_t, float select_t) noexcept;

// A framed region whose content scrolls - list_view, the track list, the sky panel's body, tab_view's
// content: a child window on theme::surface with a theme::border frame, `padding` inside (theme::space).
// `rounding` is theme::radius::scroll unless the frame shares a corner with one around it, which then
// passes that one's radius. Returns whether the content is visible, as BeginChild; end it with
// scroll_body_end() either way.
bool scroll_body_begin(const char* id, ImVec2 size, float padding, float rounding, ImGuiWindowFlags flags = 0);
void scroll_body_end();

// A colour as a round swatch - color_field, the theme editor's rows: `diameter` across, the colour inside a
// theme::border ring, glowing in its own hue `hover_t` of the way. A colour that is not opaque sits on a
// checkerboard, so a half-transparent one does not pass for a darker opaque one (review R16).
void color_swatch(ImDrawList* draw, const ImVec2& center, float diameter, const ImVec4& color, float hover_t) noexcept;

// The start of a "label + control" row: `label` at the left, centred on the row's height (the taller of
// the control and a line of text) and cut with an ellipsis before the control, and the cursor parked at the
// right edge where a control of `control` size goes, centred the same way. Draw the control next. Returns
// the label text's rect, for a caller that adds something after it (the Tweaks tab's "QP").
//
// The control's real size rather than a guess at it: a literal 44 for a 40-pixel toggle left every toggle
// four pixels short of the edge, and a label sitting at the top of a taller control read as misaligned.
ImRect label_row(const char* label, const ImVec2& control);

// label_row for a label that is a sentence rather than a name - the Quick Player's tag descriptions: the
// label wraps onto as many lines as it needs instead of being cut, and the control sits at the right edge,
// centred on the label's first line. Whatever comes next lands under the label, at the row's left edge.
ImRect label_block(std::string_view label, const ImVec2& control);

// A small label on its own ground - a Quick Player track's tags, a Scripts row's state. One geometry, two
// colourings: neutral (theme::surface_badge under theme::text_muted) for labels that only inform, and
// tinted (the tint at 18% under the tint itself) for labels whose colour is the message. pill_size() is
// the whole pill, padding included, for laying a run of them out before drawing.
ImVec2 pill_size(const char* text) noexcept;
float pill_height() noexcept;
void pill(ImDrawList* draw, const ImVec2& min, const char* text) noexcept;
void pill(ImDrawList* draw, const ImVec2& min, const char* text, const ImVec4& tint) noexcept;

// Begin() of a window the overlay draws itself - the menu, the sky parameter panel - and its chrome: the
// body on theme::surface in a theme::border_window frame, theme::radius::window, and a theme::space::title_h
// title bar with `title` in it, cut with an ellipsis where it would run into the close_button a `closable`
// window has. Position and size come from SetNextWindowPos/Size as usual. ImGui's own background, border
// and nav highlight are off: a NoBackground window still draws a square border and a nav rect, and both
// peek out from under the rounded corners. Returns the title bar's rect; end with ImGui::End().
ImRect window_begin(const char* name, const char* title, bool closable = false);

// A window's close button at the right end of its title bar: where it goes (for kit::interact), and the
// button drawn there with the pointer `hover_t` of the way onto it.
ImRect close_button_rect(const ImRect& title) noexcept;
void close_button(ImDrawList* draw, const ImRect& slot, float hover_t) noexcept;

// The resize grip in a window's bottom-right corner: where it answers (for kit::interact), and its mark -
// two arcs concentric with the window's rounded corner rather than a triangle at the corner point, which
// would poke out past the rounded silhouette there.
ImRect resize_grip_rect(const ImVec2& window_max) noexcept;
void resize_grip(ImDrawList* draw, const ImVec2& window_max) noexcept;

// Draws `text` clipped to end at `max_x`, ending in an ellipsis when it does not fit. Returns whether the
// text was cut - for a caller that offers the whole of it in a tooltip.
bool add_text_ellipsis(ImDrawList* draw, const ImVec2& pos, float max_x, ImU32 col, const char* text) noexcept;

// The same cut as a string, for text that is drawn some other way (with a glow, at another size): `text`
// itself when it fits in `max_w`, otherwise as much of it as fits followed by "...". Allocates - cache the
// result against the width it was made for rather than calling this every frame.
std::string fit_text(const char* text, float max_w);

// Soft glow via offset AddText copies (no shaders). strength in [0, 1].
//
// `font`/`font_size` pick the face and pixel size, exactly as the AddText overload they are handed
// to does. Passing null/<=0 means "whatever the current window is using", which is what every
// widget in the overlay wants; the explicit form exists for the script HUD, which chooses both.
void add_text_glow(ImDrawList* draw,
    const ImVec2& pos,
    const char* text,
    ImU32 text_col,
    ImVec4 glow_col,
    float strength,
    ImFont* font = nullptr,
    float font_size = 0.f) noexcept;

// How far add_rect_glow and add_circle_glow reach past the shape: the outermost outline's offset plus half
// its thickness. A glow is drawn where it is clipped, and the clip of a window ends at the window's edge -
// so whatever glows needs this much room inside the clip: a page's padding (tab_view), or a clip rect
// widened by this much where the shape is the window itself (popup_menu).
inline constexpr float glow_reach = 6.f + 0.75f;

// Soft glow around a rounded rect border via several expanding, fading outline copies (no
// shaders). strength in [0, 1].
void add_rect_glow(ImDrawList* draw, const ImVec2& p_min, const ImVec2& p_max, float rounding, ImVec4 glow_col, float strength) noexcept;

// The same around a circle.
void add_circle_glow(ImDrawList* draw, const ImVec2& center, float radius, ImVec4 glow_col, float strength) noexcept;
} // namespace tw::ui::kit::paint
