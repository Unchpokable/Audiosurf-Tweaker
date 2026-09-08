#pragma once

// The C ABI scripts actually call.
//
// Deliberately `extern "C"`, POD-only, and free of anything that can throw or that owns a
// destructor. Two reasons, both from Docs/Internal/lua-scripting.md:
//
//  - §2.2: these are reached from Lua through the FFI, not as lua_CFunction. A lua_CFunction call
//    is NYI for LuaJIT's trace compiler; an FFI call is compiled into the trace as a direct call.
//    That is the whole performance argument for the scripting layer, so the boundary has to look
//    like C.
//  - §2.3: a Lua error unwinds with longjmp, which skips C++ destructors in every frame it crosses.
//    Keeping this layer destructor-free makes that structurally harmless rather than a rule someone
//    has to remember.
//
// Not exported from the DLL: lua_host hands the addresses to the VM at bootstrap and the Lua side
// ffi.cast()s them, which keeps them out of the export table (and out of reach of anything that did
// not receive them deliberately).
namespace tw::lua::api
{
extern "C" {
// Why a resolve did not produce a handle. The distinction between "not yet" and "wrong" is the whole
// point: the first is the normal state during startup and must stay silent, the second is a mistake
// in the script and must be loud.
enum resolve_status : int {
    resolve_ok = 0,
    resolve_engine_pending = 1, // the engine pointer has not been captured yet - retry later
    resolve_no_group = 2,       // group not loaded (can be transient - groups load and unload)
    resolve_no_channel = 3,     // group is there, no channel by that name (usually a typo)
    resolve_wrong_kind = 4,     // the channel exists and is a different family - never transient
    resolve_unusable = 5,       // right family, but its vtable slot is not code. Should not happen
};

// Resolves a channel of an expected kind. Cold path - a name lookup is a linear _stricmp scan over
// the whole group, so callers keep the handle rather than repeating this.
//
// `kind` is tw::lua::channels::kind. `out` receives two ints: [0] a resolve_status, and [1] the kind
// the channel actually turned out to be (only meaningful on resolve_wrong_kind). Two out-ints rather
// than a struct return, for the x86 ABI reasons in Docs/Internal/lua-scripting.md §8.3 - the Lua side
// keeps one reusable cdata buffer, so this costs no allocation per call either.
//
// Returns null unless the status is resolve_ok.
void* tw_channel_resolve(const char* group, const char* name, int kind, int* out) noexcept;

// Same, addressing the channel by its index in the group instead of by name. Channel names are not
// unique - see lua_channels::find_channel_at - so an index is sometimes the only way to be precise.
void* tw_channel_resolve_at(const char* group, int index, int kind, int* out) noexcept;

// Human-readable name of a tw::lua::channels::kind, for error messages.
const char* tw_kind_name(int kind) noexcept;

// Evaluates a numeric channel through its own vtable. Cheap; safe to call every frame. The handle
// must have come from a resolve that asked for the number kind - this does not re-check.
float tw_channel_get(void* channel) noexcept;

// Writes a numeric channel. The handle must have resolved as numeric; on an Aco_FloatChannel this is
// a plain store, and what the game makes of an unexpected value is entirely up to the game (see
// Docs/Internal/lua-scripting.md §8.1).
void tw_channel_set(void* channel, float value) noexcept;

// Reads a text channel, converting from wide storage when that is the mode it is in. The returned
// pointer is valid until the next call; Lua copies it into a string on the way through the FFI, so
// scripts never see the lifetime.
const char* tw_channel_text(void* channel) noexcept;

// Reads a vector channel into three floats. The handle must have resolved as the vector kind. Zero
// on failure, leaving `out` untouched.
//
// The game keeps its live block palette in vector channels, which is what this exists for: a script
// that colours its own UI by traffic colour can take the player's actual colour settings rather than
// hardcoding a guess at them.
int tw_channel_vector(void* channel, float* out) noexcept;

// Writes a vector channel. The handle must have resolved as the vector kind - that is what keeps
// this off the numeric family's incompatible slot-19 setter (see lua_channels::set_vector).
//
// Wider effect than tw_channel_set: the engine's own SetVector also writes each component through
// into the numeric channel wired to that port, if there is one.
void tw_channel_set_vector(void* channel, float x, float y, float z) noexcept;

// Reads one cell of an Array Table column through its cursor pair. Both handles must have resolved
// as numeric channels; the cursor is saved and restored around the read.
float tw_array_read(void* array_value, void* indexer, float index) noexcept;

// Same for an `Array Vector` column: the column handle must have resolved as the vector kind, the
// cursor as numeric. Writes x/y/z into `out`; zero on failure.
int tw_array_read_vector(void* array_vector, void* indexer, float index, float* out) noexcept;

// Writes one cell of an Array Table column. Zero when the write did not happen - and unlike the
// scalar setters, "the row does not exist" is one of the reasons, because on this path the engine
// would otherwise *create* it and quietly lengthen the game's table (engine journal §2.2.3).
//
// Subject to the same write gate as tw_channel_set.
int tw_array_write(void* array_value, void* indexer, float index, float value) noexcept;
int tw_array_write_vector(void* array_vector, void* indexer, float index, float x, float y, float z) noexcept;

// How many rows that column's table has, or -1 if it cannot be asked. Exposed rather than kept
// private because the bounds check needs it anyway, and without it a script has to discover the
// length of a table the way traffic.lua does - through some unrelated counter channel the game
// happens to keep nearby.
int tw_array_rows(void* column) noexcept;

// The overlay's live palette, so a script's own chrome can match it instead of inventing colours
// that drift out of place the moment the user changes a theme.
//
// Addressed by index rather than by name: names are enumerated once at bootstrap and turned into a
// Lua lookup table, which keeps the per-frame path free of string comparisons. The colour is read
// from the theme at call time, not cached, because the Settings colour pickers edit it in place.
int tw_theme_count() noexcept;
const char* tw_theme_name(int index) noexcept;
unsigned int tw_theme_color(int index) noexcept;

// Subscribes to a channel's CallChannel. Any family is accepted - the Do_* handlers that carry the
// game's events are ChannelCaller channels, which have no accessor of their own.
//
// `after` selects the phase: non-zero means the callback runs once the original has returned, which
// is what an event consumer wants (the handler's writes have landed). `out` carries a resolve_status
// exactly as tw_channel_resolve does.
//
// Returns a subscription id >= 0, or -1. The id comes back to Lua as the argument of
// __tw_dispatch_call, which is how the callback is found again without C ever holding a Lua value.
// `owner` is the id of the script that asked, so the subscription can be taken back out when that
// script is disabled without disturbing anyone else's - see tw_unsubscribe_owner. -1 means unowned.
int tw_on_call(int owner, const char* group, const char* name, int after, int* out) noexcept;

// Same, by channel index.
int tw_on_call_at(int owner, const char* group, int index, int after, int* out) noexcept;

// Takes a channel out of the graph: its CallChannel is intercepted and the engine's own handler is
// never run. The channel keeps its place in the graph and its parents keep calling it - they simply
// get nothing back.
//
// This is `tw_on_call` with a "before" handler that always declines, minus the Lua round trip. It
// exists separately because the thing worth suppressing is usually a render node called once per
// frame forever, and crossing into the VM sixty times a second to answer "no" is pure cost.
//
// What it is safe on is entirely a question of which channel is picked: a node whose only effect is
// drawing can be removed with no consequence, while one whose result something else reads leaves
// that reader holding a stale value. Nothing here can tell the two apart.
//
// Returns a subscription id (the same space as tw_on_call, so tw_on_call_clear releases both), or -1
// with `out` carrying a resolve_status.
int tw_mute(int owner, const char* group, const char* name, int* out) noexcept;
int tw_mute_at(int owner, const char* group, int index, int* out) noexcept;

// Turns a mute on or off without unhooking. The vtable stays swapped either way - re-hooking per
// toggle would mean a fresh vtable copy each time, and toggling is exactly what a script does when
// the feature it replaces goes on and off.
void tw_mute_set(int id, int enable) noexcept;

// Drops every CallChannel subscription and puts the original vtables back.
// Drops every subscription belonging to one script, and restores the vtable of every channel that
// thereby lost its last subscriber. This is what "disable a script" means at this layer: not a
// dormant hook that returns early, but no hook at all, so a disabled script costs the game exactly
// nothing. Other scripts watching the same channels keep working.
void tw_unsubscribe_owner(int owner) noexcept;

// Drops every CallChannel subscription and puts the original vtables back.
void tw_on_call_clear() noexcept;

// How many subscriptions exist, in total (owner < 0) or for one script.
int tw_subscription_count(int owner) noexcept;

// How many distinct channels currently carry more than one subscription - i.e. in how many places
// two scripts are sharing an interception point.
int tw_shared_channel_count() noexcept;

// Diagnostics for "my group did not resolve": how many groups are loaded, and what each one is
// called. The engine stores a full path, so what a script types ("Puzzle") matches neither that nor
// necessarily the pool name - being able to print the real list is what turns a silent miss into an
// obvious one.
int tw_group_count() noexcept;
const char* tw_group_name(int index) noexcept;

// Whether writes to the graph are currently accepted.
//
// False while the game is still assembling itself, when a write does not crash anything but does
// silently corrupt it - see the write gate in lua_api.cxx. Exposed so a script can wait deliberately
// rather than have its writes dropped without knowing why.
int tw_can_write() noexcept;

// Called once per frame by lua_host, before dispatch. Drives the write gate: it watches how long the
// set of loaded channel groups has been unchanged, which is what "loading has finished" looks like
// from outside without needing to know any of the game's state values.
void tick() noexcept;

// Whether the graph is reachable at all - false until framework/channel_hook captures
// EngineInterface*, which can take until the player touches a menu (see lua-scripting.md §7).
int tw_engine_ready() noexcept;

// Seconds elapsed since the previous frame, from the same source the overlay's own animations use
// (ui/widgets/detail/draw.hxx: dt_ms, which carries the sub-millisecond remainder forward instead
// of rounding it away - at the several thousand FPS an uncapped window reaches, rounding makes every
// animation run at a multiple of its intended speed).
float tw_dt() noexcept;

// One of tweeny's easing curves evaluated at `t`, clamped to [0, 1] and mapped to [0, 1].
//
// A pure function, deliberately: the alternative was exposing tweeny's tween objects, which carry
// state and destructors and would need a per-script handle pool with the same ownership machinery
// the channel subscriptions already have. A script keeping its own `t` and calling this needs none
// of that, and it disappears with the script for free. See Docs/Internal/lua-scripting.md.
//
// `curve` indexes tw_ease_name below; out of range is linear.
float tw_ease(int curve, float t) noexcept;

int tw_ease_count() noexcept;
const char* tw_ease_name(int index) noexcept;

// Writes to the plugin log. Stripped in release builds like every other TW_LOG_* call.
void tw_log(const char* message) noexcept;

// Raises a notefeed toast - the overlay's existing transient notification strip.
void tw_notify(const char* message) noexcept;

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

// Fills `out` with the addresses above, in the order the bootstrap chunk expects. Kept next to the
// declarations so the two cannot drift apart silently.
[[nodiscard]] std::span<void* const> entry_points() noexcept;
} // namespace tw::lua::api
