#pragma once

// Where on the HUD something already is, this frame: tw.hud.occupied, tw.hud.claim and tw.hud.place
// (Docs/Internal/script-coexistence.md §4).
//
// **Measured, not declared.** Every tw.hud.* drawing call notes the box of what it drew against the script
// drawing it (api_hud.cxx measures the vertices it added to the draw list); boxes of one script that touch
// or nearly touch are merged, so a column of text lines is one box and a heatmap of hundreds of cells is one
// box too. The overlay's own chrome - notefeed, pins, watermark, the menu - is handed in at the start of the
// frame. A script asking sees the chrome and everything the scripts above it in the run order drew this
// frame; scripts below have not drawn yet, and are the ones expected to make room.
//
// No ImGui here, so the harness (harness/hudspace) runs it as it is. Render thread only. The per-frame path
// - note() on every drawing call - allocates nothing: each script has a fixed number of boxes, and running
// out merges the two that cost least to merge.
namespace tw::lua::hud_space
{
struct rect {
    float x0;
    float y0;
    float x1;
    float y1;
};

// Who a box belongs to: a script id (>= 0), or one of these for the overlay's chrome - the same widgets
// tw_hud_widget_rect knows, numbered -1 - hud_widget.
enum chrome : int {
    chrome_notefeed = -1,
    chrome_pins = -2,
    chrome_watermark = -3,
    chrome_menu = -4,
};

struct entry {
    rect box;
    int by;
};

// Corners for place(), as tw.hud.place names them.
enum corner : int {
    corner_top_left = 0,
    corner_top_right = 1,
    corner_bottom_left = 2,
    corner_bottom_right = 3,
};

// How close two boxes of one script may be and still be one box, in pixels.
inline constexpr float k_merge_gap = 6.f;

// How many boxes one script keeps per frame before it starts merging the nearest.
inline constexpr int k_boxes_per_script = 16;

// Starts a frame: forgets every script's boxes, takes the chrome as it is laid out now, and the screen
// (`screen`), which every box is clipped to and place() searches. `scripts` is how many ids there can be.
// Cold: once per overlay frame, before any script draws - and the only call that can allocate, when the
// number of scripts has grown.
void begin_frame(rect screen, std::span<const entry> chrome, int scripts);

// What one drawing call of script `owner` covered. Owner < 0 - the layer, or no script running - is not
// recorded. Empty and inverted boxes are dropped; the rest is clipped to the screen.
void note(int owner, rect box) noexcept;

// Writes what is taken, as `owner` sees it, into `out`: the chrome, then every script's boxes in the
// order the scripts drew - except `owner`'s own. Returns how many were written.
int occupied(int owner, std::span<entry> out) noexcept;

// The nearest spot to `where` for a `width` x `height` box: inside the screen, `margin` away from its edges
// and from every box drawn or claimed this frame - `owner`'s own included, so two calls of one script do not
// stack - and from the chrome, except the menu: it comes and goes as the user opens it, and a HUD that jumped
// every time would be worse than one the menu covers. On success the spot is taken for `owner` as if it had
// drawn there, and its top-left corner is in `out`.
bool place(int owner, float width, float height, corner where, float margin, float* out) noexcept;
} // namespace tw::lua::hud_space
