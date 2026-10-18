#pragma once

#include <imgui.h>

#include <string>
#include <string_view>

namespace tw::ui::theme
{
// Accent (Dark defaults; RW — mutate at runtime / via from_config)
inline ImVec4 accent_primary {};
inline ImVec4 accent_secondary {};
inline ImVec4 accent_text {};
inline ImVec4 accent_selection {};
inline ImVec4 accent_soft {};
inline ImVec4 accent_hover {};
inline ImVec4 accent_pressed {};
inline ImVec4 accent_border {};
inline ImVec4 accent_selected {};
inline ImVec4 accent_ghost {};

// Surfaces
inline ImVec4 surface {};
inline ImVec4 surface_muted {};
inline ImVec4 surface_soft {};
inline ImVec4 surface_hover {};
inline ImVec4 surface_input {};
inline ImVec4 surface_row {};
inline ImVec4 surface_row_hover {};
inline ImVec4 surface_skeleton {};
inline ImVec4 surface_badge {};
inline ImVec4 surface_elevated {};
inline ImVec4 control_track_off {};
inline ImVec4 control_thumb {};

// Borders
inline ImVec4 border {};
inline ImVec4 border_subtle {};
inline ImVec4 border_strong {};
inline ImVec4 border_divider {};
inline ImVec4 border_window {};

// Text
inline ImVec4 text_primary {};
inline ImVec4 text_secondary {};
inline ImVec4 text_muted {};
inline ImVec4 text_subtle {};
inline ImVec4 text_faint {};
inline ImVec4 text_glyph_muted {};
inline ImVec4 text_on_accent {};

// Diagnostic text. Here rather than as literals at the call site because the rule is that every
// colour comes from the theme every frame - and because a user theme that recolours the whole
// overlay has no way to reach a hard-coded red buried in the Skybox tab.
inline ImVec4 text_error {};
inline ImVec4 text_warning {};

// Metrics. Constants of the design rather than settings: the theme editor edits colours only, so
// these are not in from_config()/to_config(). Docs/Internal/tweaker-plugin-widgets.md, "Тема".
namespace space
{
// A window the overlay draws itself (kit::paint::window_begin): its title bar's height, and the padding
// between its frame and what it holds.
inline constexpr float title_h = 32.f;
inline constexpr float window_pad = 10.f;

// Inside a scrolling list's frame, around its cards: list_view, the Quick Player track list.
inline constexpr float list_pad = 6.f;

// Inside a frame of controls: a tab_view page, the sky parameter panel's body. Wider than list_pad because
// controls glow (a slider's thumb, a colour swatch) and the frame clips at its edge - see
// kit::paint::glow_reach.
inline constexpr float content_pad = 8.f;

// Around the tab buttons in a tab_view's navigation column.
inline constexpr float nav_pad = 6.f;

// Between a segmented strip's track and the chips inside it.
inline constexpr float strip_pad = 3.f;

// Between two cards of a list. Exact: the lists that place their rows themselves (the track list's
// prefix sum) count it into their layout rather than leaving it to ImGui's ItemSpacing.
inline constexpr float row_gap = 6.f;
} // namespace space

// Corner radii. The rule (review R17): a shape nested close inside another, near its corner, has the
// outer radius less the gap between them - otherwise the gap is visibly wider or narrower at the corner
// than along the sides. So the radii that frame something are written as what they hold plus the gap,
// and changing a card's radius moves its list's frame with it. Where the gap is as wide as a window's
// padding the rule asks for nothing usable, and the outer radius only has to be at least the inner.
namespace radius
{
// A row of a list, which is a card (kit::paint::row_background): list_item, a Quick Player track, a
// Scripts row.
inline constexpr float row = 6.f;

// Something clicked: button, a segmented strip's track.
inline constexpr float control = 8.f;

// A segmented strip's chip, inside its track.
inline constexpr float chip = control - space::strip_pad;

// A tab_view's tab button, in the navigation column.
inline constexpr float tab = 4.f;

// Something typed into: number_input's frame, slider's edit field.
inline constexpr float field = 6.f;

// A small label on its own ground (kit::paint::pill): a track's tags, a script's state.
inline constexpr float pill = 5.f;

// A scrolling list's frame, around its cards: list_view, the Quick Player track list.
inline constexpr float scroll = row + space::list_pad;

// A framed region inside a window, around its tab buttons: tab_view (and item_group, the same kind of
// frame).
inline constexpr float panel = tab + space::nav_pad;

// A window the overlay draws itself: the menu, the sky parameter panel, a popup_menu. Its padding is too
// wide for the rule; it is the panel's, so a window is never less round than the panel it holds.
inline constexpr float window = panel;
} // namespace radius

// How far a control that cannot be used right now fades (button, toggle, segmented with set_enabled(false)):
// still legible, plainly not live. A multiplier on the style alpha, so it composes with a page fading.
inline constexpr float disabled_alpha = 0.4f;

// How a value moves from where it is to where it is going. Pure data - ui/kit/anim turns it into a
// tween - so this header stays free of the animation library.
enum class easing {
    cubic_out,
    quadratic_out,
};

struct timing {
    int duration_ms;
    easing curve;
};

namespace motion
{
// Pointer arrives on or leaves an element.
inline constexpr timing hover { 150, easing::cubic_out };

// Mouse button goes down or comes up on an element. Shorter than hover: a press has to answer
// before the release that may follow it a few frames later.
inline constexpr timing press { 90, easing::quadratic_out };

// An element becomes or stops being the selected one.
inline constexpr timing select { 150, easing::cubic_out };

// A toggle's thumb crossing its track.
inline constexpr timing slide { 200, easing::cubic_out };

// Whole content appearing or giving way to other content: tab pages crossfading, a popup fading.
inline constexpr timing transition { 200, easing::cubic_out };
} // namespace motion

void apply_dark() noexcept;
void from_config(std::string_view config) noexcept;

// ImGui's own style from this theme - what ImGui draws itself and the widgets do not repaint: scrollbars,
// tooltips, Separator, text fields (their frame, selection, caret), TextDisabled (review R9).
//
// apply_to() is the whole of it, colours and metrics, once on a fresh context - in place of
// StyleColorsDark(), which it starts from, so the few colours no theme token names stay ImGui's.
// sync_colors() is the colours alone, at the start of every frame: the theme editor writes into the theme
// while a picker is dragged, Default resets it, a config load replaces it, and none of those has to
// know the style exists. Metrics are not re-applied - whoever scaled them (the smoke window, by DPI)
// keeps the scale.
void apply_to(ImGuiStyle& style) noexcept;
void sync_colors(ImGuiStyle& style) noexcept;

// Serializes the current (possibly live-edited) theme back to the same `key=#AARRGGBB` per-line
// format that from_config() parses - used to persist runtime color-picker edits.
[[nodiscard]] std::string to_config() noexcept;
} // namespace tw::ui::theme
