#pragma once

// Drawing: the tw.hud.* half of the C ABI, plus the overlay's palette and font faces so a script's
// chrome can match the overlay's. See api_channels.hxx for why everything here looks like C.
//
// None of this pushes anything onto an ImGui stack - every call adds to the background draw list and
// returns. That is what lets the scheduler run each script's on_frame under its own pcall without an
// ImGui recovery snapshot per script: a Lua error cannot land in the middle of a C function, so there
// is no half-pushed state for one script to leave behind for the next (lua_sched.cxx).
namespace tw::lua::api
{
extern "C" {
// The overlay's live palette, so a script's own chrome can match it instead of inventing colours
// that drift out of place the moment the user changes a theme.
//
// Addressed by index rather than by name: names are enumerated once at bootstrap and turned into a
// Lua lookup table, which keeps the per-frame path free of string comparisons. The colour is read
// from the theme at call time, not cached, because the Settings colour pickers edit it in place.
int tw_theme_count() noexcept;
const char* tw_theme_name(int index) noexcept;
unsigned int tw_theme_color(int index) noexcept;

// Draws on the ImGui background draw list: over the game, under the overlay's own windows, and
// visible whether or not the menu is open. `color` is ImGui's packed ABGR (IM_COL32 order).
//
// Scripts draw after every overlay widget has run (see ui_main::draw_frame), so what they put here
// lands on top of the overlay's own chrome rather than under it. That makes the geometry below a
// layout aid rather than a keep-out map.
void tw_hud_text(float x, float y, unsigned int color, const char* text) noexcept;

// Text at an explicit pixel size, rather than the overlay's own. `size` <= 0 means "the default".
//
// Font size is a layout tool, not decoration: a widget that has to line a large heading up against a
// stack of small rows needs to choose both sizes and then measure them, which is what this and
// tw_hud_measure are for together.
void tw_hud_text_sized(float x, float y, unsigned int color, const char* text, float size) noexcept;

// Width and height that tw_hud_text_sized would occupy, into out[0] and out[1]. Same `size`
// convention. The measurement is ImGui's own, so it agrees with what gets drawn.
void tw_hud_measure(const char* text, float size, float* out) noexcept;

// The baked font faces, by index - enumerated once at bootstrap and turned into a Lua name->index
// table, the same shape as the theme palette above, so the per-frame path never compares strings.
//
// Weights rather than sizes: ImGui rasterizes a face at whatever size it is drawn at, so `size` and
// `font` are independent axes. An out-of-range index falls back to face 0 rather than failing, so a
// script written against a weight a future build drops still draws.
int tw_font_count() noexcept;
const char* tw_font_name(int index) noexcept;

// tw_hud_text_sized / tw_hud_measure with an explicit face. The two must agree, which is why the
// measuring half takes the face too - measuring semibold and drawing regular is off by enough to be
// visible in a right-aligned column.
void tw_hud_text_font(float x, float y, unsigned int color, const char* text, float size, int font) noexcept;
void tw_hud_measure_font(const char* text, float size, int font, float* out) noexcept;

// One packed SVG icon, drawn into a `size` x `size` box at (x, y), tinted.
//
// `name` is a bare stem: "feat_stealth" reaches "icons/feat_stealth.svg". The prefix and extension
// are attached here rather than taken from the script so a script cannot address a different
// resource class - the icons are the only asset a script is given, and this is what makes that
// true rather than merely intended.
//
// Icons are authored monochrome (white plus alpha) and coloured by `tint`, like every other icon in
// the overlay. An unknown name draws nothing.
void tw_hud_icon(const char* name, float x, float y, float size, unsigned int tint) noexcept;

// A rectangle, optionally with rounded corners. `thickness` <= 0 fills it; anything else strokes an
// outline of that width.
void tw_hud_rect(float x0, float y0, float x1, float y1, unsigned int color, float rounding, float thickness) noexcept;

// Same, but rounding only the corners named by `corners`, as a bitmask of hud_corner below.
//
// This is what a segmented bar needs: a fill made of several rectangles has to round the outer ends
// and leave the internal joins square, or the joins read as gaps and the last segment's square
// corner escapes from under the rounded outline drawn over it.
//
// NOTE the translation this performs, because it is not the identity. ImGui's own
// ImDrawFlags_RoundCornersNone is `1 << 8`, and *zero* means RoundCornersAll - so passing 0 through
// unchanged would round everything for a caller that asked for nothing. Scripts therefore get a
// plain four-bit mask where 0 honestly means "no rounding", and this maps it.
enum hud_corner : int {
    hud_corner_top_left = 1,
    hud_corner_top_right = 2,
    hud_corner_bottom_left = 4,
    hud_corner_bottom_right = 8,

    hud_corner_all = 15,
};

void tw_hud_rect_corners(
    float x0, float y0, float x1, float y1, unsigned int color, float rounding, float thickness, int corners) noexcept;

// A soft glow around a rounded rect, as several expanding fading outline copies - the overlay's own
// technique (ui/widgets/detail/draw.hxx), shared rather than reimplemented so a script's glow looks
// like the overlay's. `strength` is 0..1; <= 0.01 draws nothing.
//
// Draws only the glow, not the rect: a script that wants both fills first and glows after, which is
// also the order that lets it glow in a different colour than it fills.
void tw_hud_rect_glow(float x0, float y0, float x1, float y1, unsigned int color, float rounding, float strength) noexcept;

// A rectangle filled with a two-stop linear gradient, `from` at the low edge and `to` at the high
// one, horizontal unless `vertical` is non-zero. Both colours carry their own alpha, so fading a
// backdrop out to nothing is the same call as fading one colour into another.
//
// No rounding: the underlying primitive is a single quad with per-corner colours, and ImGui has no
// rounded form of it. A gradient that needs a soft end gets it from the gradient, not from a radius.
void tw_hud_rect_gradient(
    float x0, float y0, float x1, float y1, unsigned int from, unsigned int to, int vertical) noexcept;

// Text with a glow behind it. Draws the text too - unlike the rect version, because the glow is
// offset copies of the same glyphs and doing it in two calls would rasterize them twice.
void tw_hud_text_glow(float x,
    float y,
    unsigned int text_color,
    unsigned int glow_color,
    const char* text,
    float size,
    int font,
    float strength) noexcept;

// A line segment.
void tw_hud_line(float x0, float y0, float x1, float y1, unsigned int color, float thickness) noexcept;

// Screen geometry a script needs to lay itself out, selected by index (see hud_metric below).
//
// One indexed getter rather than a struct return or out-parameters: returning small structs by value
// has x86 calling-convention corners the FFI does not need to be dragged through, and out-parameters
// would force every script to allocate a cdata array for a number. New metrics get appended; the
// numbering is part of the ABI.
enum hud_metric : int {
    hud_viewport_width = 0,
    hud_viewport_height = 1,

    // The rectangle left over once the overlay's own always-on chrome is excluded. Kept as the
    // coarse answer for scripts that just want somewhere uncluttered to sit; tw_hud_widget_rect is
    // the precise one.
    hud_safe_x0 = 2,
    hud_safe_y0 = 3,
    hud_safe_x1 = 4,
    hud_safe_y1 = 5,

    // The overlay's default text height. Scripts sizing their own layout should scale off this
    // rather than assume 13px - the overlay bakes its font at whatever size the theme asks for.
    hud_font_size = 6,
};

float tw_hud_metric(int which) noexcept;

// Which overlay widget tw_hud_widget_rect is being asked about. Appended to, never renumbered.
enum hud_widget : int {
    hud_widget_notefeed = 0,  // the toast column - reported as a reserved strip, always present
    hud_widget_pins = 1,      // the tweak/skin pin stack - absent when no tweak is on
    hud_widget_watermark = 2, // the "Audiosurf Tweaker" badge
    hud_widget_menu = 3,      // the settings window - absent while it is closed
};

// Where one overlay widget is this frame, as x0/y0/x1/y1 in out[0..3]. Returns 0 when the widget is
// not on screen, leaving `out` untouched.
//
// The rectangles are this frame's, not the previous one's, because scripts run after every widget
// has drawn. A script can therefore dock to the notefeed or sit beside an open menu and stay put
// while the menu is dragged, instead of trailing it by a frame.
int tw_hud_widget_rect(int which, float* out) noexcept;
}
} // namespace tw::lua::api
