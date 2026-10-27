#include "pch.hxx"

#include "lua/api/api_hud.hxx"

#include "lua/lua_hud_space.hxx"
#include "lua/lua_registry.hxx"
#include "lua/lua_sched.hxx"

#include "ui/fonts.hxx"
#include "ui/image/svg.hxx"
#include "ui/kit/paint.hxx"
#include "ui/plugins/interactive/menu.hxx"
#include "ui/plugins/static/notefeed.hxx"
#include "ui/plugins/static/pins.hxx"
#include "ui/plugins/static/watermark.hxx"
#include "ui/theme.hxx"

#include <imgui.h>
#include <imgui_internal.h>

namespace
{
// Script corner mask -> ImDrawFlags. Not a cast: ImGui's "no corners" is 1<<8 and its zero means
// "all corners", so a script that passed 0 meaning none would get all of them, silently. See the
// note on hud_corner in api_hud.hxx.
ImDrawFlags to_draw_flags(int corners) noexcept
{
    if((corners & tw::lua::api::hud_corner_all) == 0) {
        return ImDrawFlags_RoundCornersNone;
    }

    ImDrawFlags flags = 0;
    if((corners & tw::lua::api::hud_corner_top_left) != 0) {
        flags |= ImDrawFlags_RoundCornersTopLeft;
    }
    if((corners & tw::lua::api::hud_corner_top_right) != 0) {
        flags |= ImDrawFlags_RoundCornersTopRight;
    }
    if((corners & tw::lua::api::hud_corner_bottom_left) != 0) {
        flags |= ImDrawFlags_RoundCornersBottomLeft;
    }
    if((corners & tw::lua::api::hud_corner_bottom_right) != 0) {
        flags |= ImDrawFlags_RoundCornersBottomRight;
    }

    return flags;
}

// Whether it is safe to touch ImGui at all right now, and specifically to add to a draw list.
//
// Scripts draw from on_frame, which the overlay calls from inside its own ImGui frame - fine. But
// nothing stops a script calling tw.hud.text from an on_call handler instead, and those run on the
// engine's call stack, in the middle of the game's own logic, with no frame open and possibly before
// the overlay has an ImGui context at all. Adding to a draw list there is at best output that is
// discarded at the next NewFrame and at worst a null dereference inside the game.
//
// So the drawing entry points check, and quietly do nothing outside a frame. Quietly, because the
// alternative - a Lua error - would fire from inside the game's graph walk, every call, and count
// towards suspending a script whose only mistake was drawing from the wrong callback.
[[nodiscard]] bool inside_frame() noexcept
{
    const ImGuiContext* context = ImGui::GetCurrentContext();
    return context != nullptr && context->WithinFrameScope;
}

// What one drawing call covered, for tw.hud.occupied (lua_hud_space): the box of the vertices it added
// to the background draw list, noted against the script whose callback is drawing. Measured from what was
// drawn rather than from the arguments, so text, glow, icons and thick lines all come out right without a
// second measurement of their own - it costs a min/max per vertex the call added.
//
// Something fully transparent is not recorded - it occupies nothing anyone can see - and neither is
// drawing outside any script (owner -1).
class space_probe {
public:
    explicit space_probe(bool visible) noexcept : m_owner(visible ? tw::lua::sched::current_owner() : -1)
    {
        if(m_owner >= 0) {
            m_list = ImGui::GetBackgroundDrawList();
            m_start = m_list->VtxBuffer.Size;
        }
    }

    space_probe(const space_probe&) = delete;
    space_probe& operator=(const space_probe&) = delete;

    ~space_probe()
    {
        if(m_owner < 0 || m_list == nullptr) {
            return;
        }

        const int end = m_list->VtxBuffer.Size;
        if(end <= m_start) {
            return;
        }

        tw::lua::hud_space::rect box { FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX };
        for(int i = m_start; i < end; ++i) {
            const ImVec2 pos = m_list->VtxBuffer.Data[i].pos;
            box.x0 = std::min(box.x0, pos.x);
            box.y0 = std::min(box.y0, pos.y);
            box.x1 = std::max(box.x1, pos.x);
            box.y1 = std::max(box.y1, pos.y);
        }

        tw::lua::hud_space::note(m_owner, box);
    }

private:
    int m_owner;
    ImDrawList* m_list = nullptr;
    int m_start = 0;
};

// tw_hud_occupied's list on its way to the script's arrays. As big as the prelude's own arrays are.
std::array<tw::lua::hud_space::entry, 256> g_occupied {};

[[nodiscard]] bool opaque(unsigned int color) noexcept
{
    return (color >> IM_COL32_A_SHIFT) != 0;
}

// A polyline's points into the draw list's own path buffer - the one ImGui's Path* calls build their lines in.
// It grows once and is reused, so a line drawn every frame allocates nothing after the first. Refuses a
// point that is not a finite number: a NaN poisons the normals of both segments it touches, and what ImGui
// then draws is not a line with a gap but triangles across the screen.
[[nodiscard]] bool fill_path(ImDrawList* list, const float* xs, const float* ys, int count) noexcept
{
    list->PathClear();
    for(int i = 0; i < count; ++i) {
        if(!std::isfinite(xs[i]) || !std::isfinite(ys[i])) {
            list->PathClear();
            return false;
        }
        list->_Path.push_back(ImVec2(xs[i], ys[i]));
    }
    return true;
}

// Where one overlay widget is this frame - for tw_hud_widget_rect, and as chrome for lua_hud_space.
bool widget_rect(int which, float& x0, float& y0, float& x1, float& y1) noexcept
{
    switch(which) {
    case tw::lua::api::hud_widget_notefeed:
        // Always reported, even with no toasts alive: the strip is a reservation, not a measurement
        // (see notefeed::reserved_rect).
        tw::ui::plugins::statics::notefeed::reserved_rect(x0, y0, x1, y1);
        return true;
    case tw::lua::api::hud_widget_pins:
        return tw::ui::plugins::statics::pins::last_rect(x0, y0, x1, y1);
    case tw::lua::api::hud_widget_watermark:
        return tw::ui::plugins::statics::watermark::last_rect(x0, y0, x1, y1);
    case tw::lua::api::hud_widget_menu: {
        ImVec2 pos {};
        ImVec2 size {};
        if(!tw::ui::plugins::interactive::menu::window_rect(pos, size)) {
            return false;
        }
        x0 = pos.x;
        y0 = pos.y;
        x1 = pos.x + size.x;
        y1 = pos.y + size.y;
        return true;
    }
    default:
        return false;
    }
}

// Weaker check for the read-only geometry calls: they do not mutate anything, so they only need a
// context to exist. A script asking for the viewport size from a channel hook gets a real answer.
[[nodiscard]] bool imgui_ready() noexcept
{
    return ImGui::GetCurrentContext() != nullptr;
}

// The overlay's palette, by name. Pointers rather than copies because the theme is live - it is
// re-applied by from_config() and edited by the Settings colour pickers, and a script asking for
// "surface" wants what the overlay is using right now.
//
// The whole set is exposed rather than a curated subset: the point is that a script's chrome matches
// the overlay's, and deciding for the author which halves of the palette they are allowed to match
// would just push them back to inventing their own colours.
struct theme_entry {
    const char* name;
    const ImVec4* color;
};

const theme_entry g_theme[] = {
    { "accent_primary", &tw::ui::theme::accent_primary },
    { "accent_secondary", &tw::ui::theme::accent_secondary },
    { "accent_text", &tw::ui::theme::accent_text },
    { "accent_selection", &tw::ui::theme::accent_selection },
    { "accent_soft", &tw::ui::theme::accent_soft },
    { "accent_hover", &tw::ui::theme::accent_hover },
    { "accent_pressed", &tw::ui::theme::accent_pressed },
    { "accent_border", &tw::ui::theme::accent_border },
    { "accent_selected", &tw::ui::theme::accent_selected },
    { "accent_ghost", &tw::ui::theme::accent_ghost },
    { "surface", &tw::ui::theme::surface },
    { "surface_muted", &tw::ui::theme::surface_muted },
    { "surface_soft", &tw::ui::theme::surface_soft },
    { "surface_hover", &tw::ui::theme::surface_hover },
    { "surface_input", &tw::ui::theme::surface_input },
    { "surface_row", &tw::ui::theme::surface_row },
    { "surface_row_hover", &tw::ui::theme::surface_row_hover },
    { "surface_skeleton", &tw::ui::theme::surface_skeleton },
    { "surface_badge", &tw::ui::theme::surface_badge },
    { "surface_elevated", &tw::ui::theme::surface_elevated },
    { "control_track_off", &tw::ui::theme::control_track_off },
    { "control_thumb", &tw::ui::theme::control_thumb },
    { "border", &tw::ui::theme::border },
    { "border_subtle", &tw::ui::theme::border_subtle },
    { "border_strong", &tw::ui::theme::border_strong },
    { "border_divider", &tw::ui::theme::border_divider },
    { "border_window", &tw::ui::theme::border_window },
    { "text_primary", &tw::ui::theme::text_primary },
    { "text_secondary", &tw::ui::theme::text_secondary },
    { "text_muted", &tw::ui::theme::text_muted },
    { "text_subtle", &tw::ui::theme::text_subtle },
    { "text_faint", &tw::ui::theme::text_faint },
    { "text_glyph_muted", &tw::ui::theme::text_glyph_muted },
    { "text_on_accent", &tw::ui::theme::text_on_accent },
    { "text_error", &tw::ui::theme::text_error },
    { "text_warning", &tw::ui::theme::text_warning },
};
} // namespace

namespace tw::lua::api
{
extern "C" {
int tw_theme_count() noexcept
{
    return static_cast<int>(std::size(g_theme));
}

const char* tw_theme_name(int index) noexcept
{
    if(index < 0 || index >= static_cast<int>(std::size(g_theme))) {
        return "";
    }

    return g_theme[index].name;
}

unsigned int tw_theme_color(int index) noexcept
{
    if(index < 0 || index >= static_cast<int>(std::size(g_theme))) {
        return 0;
    }

    // Read live rather than cached: the Settings colour pickers edit these in place, and a script
    // that matched the overlay once at load would drift away from it the moment a theme changed.
    //
    // **Packed as a value, not as a colour for drawing** - ColorConvertFloat4ToU32, not paint::to_u32. The
    // latter applies the style's alpha, which needs an ImGui context, and a script may ask for a theme colour
    // from its body: in the early load bodies run before the overlay has created one, and paint::to_u32 there
    // dereferenced a null GImGui and took the game down at startup (found by dev/ui_probe.lua, 20.10.2026). A
    // value the script keeps should not depend on the style of the moment anyway; tw.hud.* draws it as given.
    return ImGui::ColorConvertFloat4ToU32(*g_theme[index].color);
}

void tw_hud_text(float x, float y, unsigned int color, const char* text) noexcept
{
    if(text == nullptr || !inside_frame()) {
        return;
    }

    // Background draw list rather than a window: no chrome, no input, and it is drawn whether or not
    // the menu is open - which is what a HUD wants. See Docs/Internal/lua-scripting.md §8.3.
    const space_probe probe { opaque(color) };
    ImGui::GetBackgroundDrawList()->AddText(ImVec2(x, y), color, text);
}

void tw_hud_text_sized(float x, float y, unsigned int color, const char* text, float size) noexcept
{
    if(text == nullptr || !inside_frame()) {
        return;
    }

    ImFont* font = ImGui::GetFont();
    if(font == nullptr) [[unlikely]] {
        return;
    }

    if(size <= 0.f) {
        size = ImGui::GetFontSize();
    }

    const space_probe probe { opaque(color) };
    ImGui::GetBackgroundDrawList()->AddText(font, size, ImVec2(x, y), color, text);
}

void tw_hud_measure(const char* text, float size, float* out) noexcept
{
    if(out == nullptr) {
        return;
    }

    out[0] = 0.f;
    out[1] = 0.f;

    if(text == nullptr || !imgui_ready()) {
        return;
    }

    ImFont* font = ImGui::GetFont();
    if(font == nullptr) {
        return;
    }

    if(size <= 0.f) {
        size = ImGui::GetFontSize();
    }

    const ImVec2 measured = font->CalcTextSizeA(size, std::numeric_limits<float>::max(), 0.f, text);
    out[0] = measured.x;
    out[1] = measured.y;
}

void tw_hud_rect(float x0, float y0, float x1, float y1, unsigned int color, float rounding, float thickness) noexcept
{
    tw_hud_rect_corners(x0, y0, x1, y1, color, rounding, thickness, hud_corner_all);
}

void tw_hud_rect_corners(
    float x0, float y0, float x1, float y1, unsigned int color, float rounding, float thickness, int corners) noexcept
{
    if(!inside_frame()) {
        return;
    }

    const space_probe probe { opaque(color) };
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImDrawFlags flags = to_draw_flags(corners);

    if(thickness <= 0.f) {
        draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color, rounding, flags);
    }
    else {
        draw->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), color, rounding, flags, thickness);
    }
}

void tw_hud_rect_glow(float x0, float y0, float x1, float y1, unsigned int color, float rounding, float strength) noexcept
{
    if(!inside_frame()) {
        return;
    }

    const space_probe probe { opaque(color) && strength > 0.01f };
    tw::ui::kit::paint::add_rect_glow(
        ImGui::GetBackgroundDrawList(), ImVec2(x0, y0), ImVec2(x1, y1), rounding, ImGui::ColorConvertU32ToFloat4(color), strength);
}

void tw_hud_rect_gradient(
    float x0, float y0, float x1, float y1, unsigned int from, unsigned int to, int vertical) noexcept
{
    if(!inside_frame()) {
        return;
    }

    // Corner order is upper-left, upper-right, lower-right, lower-left. Horizontal puts `from` on
    // the two left corners; vertical puts it on the two top ones.
    const unsigned int tl = from;
    const unsigned int tr = (vertical != 0) ? from : to;
    const unsigned int br = to;
    const unsigned int bl = (vertical != 0) ? to : from;

    const space_probe probe { opaque(from) || opaque(to) };
    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y1), tl, tr, br, bl);
}

void tw_hud_text_glow(float x,
    float y,
    unsigned int text_color,
    unsigned int glow_color,
    const char* text,
    float size,
    int font,
    float strength) noexcept
{
    if(text == nullptr || !inside_frame()) {
        return;
    }

    ImFont* face = tw::ui::fonts::at(font);
    if(face == nullptr) [[unlikely]] {
        return;
    }

    const space_probe probe { opaque(text_color) || opaque(glow_color) };
    tw::ui::kit::paint::add_text_glow(ImGui::GetBackgroundDrawList(),
        ImVec2(x, y),
        text,
        text_color,
        ImGui::ColorConvertU32ToFloat4(glow_color),
        strength,
        face,
        size > 0.f ? size : ImGui::GetFontSize());
}

int tw_font_count() noexcept
{
    return tw::ui::fonts::count();
}

const char* tw_font_name(int index) noexcept
{
    return tw::ui::fonts::name(index);
}

void tw_hud_text_font(float x, float y, unsigned int color, const char* text, float size, int font) noexcept
{
    if(text == nullptr || !inside_frame()) {
        return;
    }

    ImFont* face = tw::ui::fonts::at(font);
    if(face == nullptr) [[unlikely]] {
        return;
    }

    const space_probe probe { opaque(color) };
    ImGui::GetBackgroundDrawList()->AddText(face, size > 0.f ? size : ImGui::GetFontSize(), ImVec2(x, y), color, text);
}

void tw_hud_measure_font(const char* text, float size, int font, float* out) noexcept
{
    if(out == nullptr) {
        return;
    }

    out[0] = 0.f;
    out[1] = 0.f;

    if(text == nullptr || !imgui_ready()) {
        return;
    }

    ImFont* face = tw::ui::fonts::at(font);
    if(face == nullptr) {
        return;
    }

    if(size <= 0.f) {
        size = ImGui::GetFontSize();
    }

    const ImVec2 measured = face->CalcTextSizeA(size, std::numeric_limits<float>::max(), 0.f, text);
    out[0] = measured.x;
    out[1] = measured.y;
}

void tw_hud_icon(const char* name, float x, float y, float size, unsigned int tint) noexcept
{
    if(name == nullptr || size <= 0.f || !inside_frame()) {
        return;
    }

    // Built here, not taken from the script: this is the only thing keeping "icons/*.svg" the only
    // resource class a script can address.
    char key[128];
    if(std::snprintf(key, sizeof(key), "icons/%s.svg", name) < 0) {
        return;
    }

    const tw::ui::image::svg::image icon = tw::ui::image::svg::get_resource(key);
    const ImTextureID texture = icon.at(static_cast<int>(std::lround(size)));
    if(texture == ImTextureID_Invalid) {
        return;
    }

    const space_probe probe { opaque(tint) };
    ImGui::GetBackgroundDrawList()->AddImage(
        ImTextureRef { texture }, ImVec2(x, y), ImVec2(x + size, y + size), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), tint);
}

void tw_hud_line(float x0, float y0, float x1, float y1, unsigned int color, float thickness) noexcept
{
    if(!inside_frame()) {
        return;
    }

    const space_probe probe { opaque(color) };
    ImGui::GetBackgroundDrawList()->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), color, thickness <= 0.f ? 1.f : thickness);
}

void tw_hud_polyline(const float* xs, const float* ys, int count, unsigned int color, float thickness, int closed) noexcept
{
    if(xs == nullptr || ys == nullptr || count < 2 || count > tw::lua::api::hud_polyline_max_points || !inside_frame()) {
        return;
    }

    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const space_probe probe { opaque(color) };
    if(!fill_path(list, xs, ys, count)) {
        return;
    }
    list->AddPolyline(list->_Path.Data, count, color, thickness <= 0.f ? 1.f : thickness, closed != 0 ? ImDrawFlags_Closed : 0);
    list->PathClear();
}

void tw_hud_gradient_polyline(
    const float* xs, const float* ys, const unsigned int* colors, int count, float thickness, int closed) noexcept
{
    if(xs == nullptr || ys == nullptr || colors == nullptr || count < 2 || count > tw::lua::api::hud_polyline_max_points
        || !inside_frame()) {
        return;
    }

    bool visible = false;
    for(int i = 0; i < count && !visible; ++i) {
        visible = opaque(colors[i]);
    }

    ImDrawList* list = ImGui::GetBackgroundDrawList();
    const space_probe probe { visible };
    if(!fill_path(list, xs, ys, count)) {
        return;
    }
    tw::ui::kit::paint::add_gradient_polyline(
        list, list->_Path.Data, colors, count, thickness <= 0.f ? 1.f : thickness, closed != 0 ? ImDrawFlags_Closed : 0);
    list->PathClear();
}

int tw_hud_widget_rect(int which, float* out) noexcept
{
    if(out == nullptr || !imgui_ready()) {
        return 0;
    }

    float x0 = 0.f;
    float y0 = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;
    if(!widget_rect(which, x0, y0, x1, y1)) {
        return 0;
    }

    out[0] = x0;
    out[1] = y0;
    out[2] = x1;
    out[3] = y1;

    return 1;
}

int tw_hud_occupied(float* boxes, int* by, int capacity) noexcept
{
    if(boxes == nullptr || by == nullptr || capacity <= 0) {
        return 0;
    }

    const std::span<tw::lua::hud_space::entry> buffer { g_occupied.data(),
        std::min(g_occupied.size(), static_cast<std::size_t>(capacity)) };
    const int count = tw::lua::hud_space::occupied(tw::lua::sched::current_owner(), buffer);

    for(int i = 0; i < count; ++i) {
        const tw::lua::hud_space::entry& item = g_occupied[static_cast<std::size_t>(i)];
        boxes[i * 4 + 0] = item.box.x0;
        boxes[i * 4 + 1] = item.box.y0;
        boxes[i * 4 + 2] = item.box.x1;
        boxes[i * 4 + 3] = item.box.y1;
        by[i] = item.by;
    }

    return count;
}

void tw_hud_claim(float x0, float y0, float x1, float y1) noexcept
{
    if(!inside_frame()) {
        return;
    }

    tw::lua::hud_space::note(tw::lua::sched::current_owner(), { std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1) });
}

int tw_hud_place(float width, float height, int corner, float margin, float* out) noexcept
{
    if(out == nullptr || corner < tw::lua::hud_space::corner_top_left || corner > tw::lua::hud_space::corner_bottom_right
       || !inside_frame()) {
        return 0;
    }

    const auto where = static_cast<tw::lua::hud_space::corner>(corner);
    return tw::lua::hud_space::place(tw::lua::sched::current_owner(), width, height, where, margin, out) ? 1 : 0;
}

float tw_hud_metric(int which) noexcept
{
    if(!imgui_ready()) {
        return 0.f;
    }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;

    switch(which) {
    case hud_viewport_width:
        return viewport.x;
    case hud_viewport_height:
        return viewport.y;
    case hud_font_size:
        return ImGui::GetFontSize();
    default:
        break;
    }

    // The always-on chrome lives in a band across the top: the toast column occupies one top corner
    // and the watermark the other (see overlay_config::feed_side). So the safe area is the viewport
    // with that band removed - full width, which is what matters.
    //
    // It used to shave the notefeed's whole column off one side instead, back when the feed reserved
    // full height. That made the safe area a tall box offset to one side, and anything laid out
    // against it landed a third of the screen from the corner it was aiming for. Pins are the one
    // piece of chrome this cannot express - they float mid-height on one side, and excluding them
    // would make the safe area a hole rather than a rectangle - so they are left to
    // tw_hud_widget_rect, which reports them exactly.
    float feed_x0 = 0.f;
    float feed_y0 = 0.f;
    float feed_x1 = 0.f;
    float feed_y1 = 0.f;
    tw::ui::plugins::statics::notefeed::reserved_rect(feed_x0, feed_y0, feed_x1, feed_y1);

    float top = feed_y1;

    float mark_x0 = 0.f;
    float mark_y0 = 0.f;
    float mark_x1 = 0.f;
    float mark_y1 = 0.f;
    if(tw::ui::plugins::statics::watermark::last_rect(mark_x0, mark_y0, mark_x1, mark_y1)) {
        top = std::max(top, mark_y1);
    }

    switch(which) {
    case hud_safe_x0:
        return 0.f;
    case hud_safe_y0:
        return top + 8.f;
    case hud_safe_x1:
        return viewport.x;
    case hud_safe_y1:
        return viewport.y;
    default:
        return 0.f;
    }
}
}

void hud_begin_frame() noexcept
{
    if(!imgui_ready()) {
        return;
    }

    std::array<tw::lua::hud_space::entry, 4> chrome {};
    std::size_t count = 0;
    for(const int which : { hud_widget_notefeed, hud_widget_pins, hud_widget_watermark, hud_widget_menu }) {
        tw::lua::hud_space::rect box {};
        if(widget_rect(which, box.x0, box.y0, box.x1, box.y1)) {
            chrome[count++] = { box, -1 - which };
        }
    }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    tw::lua::hud_space::begin_frame({ 0.f, 0.f, viewport.x, viewport.y }, std::span { chrome.data(), count }, tw::lua::registry::count());
}
} // namespace tw::lua::api
