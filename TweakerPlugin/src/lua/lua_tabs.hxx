#pragma once

// The tabs scripts put into the overlay menu, and the controls on them (Docs/Internal/script-tabs.md §3.5).
//
// A script **declares** its tab once - while it loads, or from tw.on_ready - and gets handles to its controls; C
// draws the tab every frame it is open, from a ui::form this file owns, and never enters Lua to do it. The script
// reads the form (get), writes it (set) and hears about what the user did (on_change, through the queue below and
// lua_sched's dispatch_ui). So a script that throws, is suspended or is reloaded cannot leave a page half-drawn:
// there is no Lua anywhere inside the menu's frame.
//
// **Handles** are slot numbers with a generation in the high bits - one int, which is what the prelude holds. A
// handle of a script that was unloaded is inert from that moment: its value reads as NaN (the prelude answers
// with the control's initial value), writes do nothing. Slots are reused only after the generation moves on, so
// an old handle never lands on somebody else's control.
//
// **Lifetime.** unload (lua_sched) calls drop_owner(): the script's tabs leave the menu's list and its handles go
// inert at once, but the forms are freed only in end_frame(), at the end of lua_host::draw_frame - a Reload from
// the Scripts tab happens inside menu::update, and the reloaded script's page may still be drawn later in that
// same frame by its old token. Drawing by a token that is no longer live draws nothing.
//
// **Order** in the menu: the user's run order of scripts (lua_registry::order), within a script the order it
// declared its tabs in. Republished whenever the registry's revision moves - a reorder, a suspension, a Resume,
// a reload - or a script declared or relabelled a tab. A tab's key is "<file>#<n>", so a reloaded script that
// declares the same tabs again gets the same keys, and the menu keeps it selected without rebuilding the strip.
//
// **Labels** that clash - case-insensitively, with a built-in tab or with another script's - are shown as
// "<script name> - <label>" on every clashing script tab (§2.3), the name being the header's @name or the file
// without .lua.
//
// Engine/render thread only.
namespace tw::lua::tabs
{
// Honest-mistake limits, not a security boundary: past them a declaration is refused with a diagnostic and an
// inert handle, a text is cut. The number of tabs is not limited (§2.1).
inline constexpr int k_max_label = 96;    // bytes of a label, a tooltip, an option, a list row
inline constexpr int k_max_text = 1024;   // bytes of a text node's paragraph
inline constexpr int k_max_options = 16;  // per choice
inline constexpr int k_max_rows = 512;    // per list
inline constexpr int k_max_nodes = 1024;  // per script, its tabs included

// Which text set_text() sets. Same numbers as the prelude's T_* constants.
enum class text_slot : int {
    label = 0,   // a control's label, a group's title, a text node's paragraph, a tab's label
    tooltip = 1, // shown when the pointer rests on the row; empty for none
};

// A new tab for script `owner`, last among its tabs. Returns its handle, or -1 (not a script).
[[nodiscard]] int tab(int owner, std::string_view label) noexcept;

// A new node on `parent` - a tab or a group - of ui::form::kind `kind`. Returns its handle, or -1: the parent is
// not live, not a container, nested too deep, or the script has k_max_nodes already.
[[nodiscard]] int add(int parent, int kind, std::string_view label) noexcept;

void set_range(int handle, double min_value, double max_value) noexcept;
void set_options(int handle, std::span<const std::string_view> labels) noexcept;
void set_items(int handle, std::span<const std::string_view> text, std::span<const std::string_view> subtext) noexcept;
void set_height(int handle, float height) noexcept;

// The value of a live node, as the form holds it; NaN for a handle that is not one (inert, a tab).
[[nodiscard]] double value(int handle) noexcept;

// Never an event, no-op when the value is the same, and lost against a gesture in progress (ui/form).
void set_value(int handle, double value) noexcept;

void set_text(int handle, text_slot which, std::string_view text) noexcept;

// `enabled` / `visible`: 1 on, 0 off, anything else leaves it as it is.
void set_flags(int handle, int enabled, int visible) noexcept;

// Whether the tab was on screen this frame or the last - for a script deciding whether a text is worth building.
[[nodiscard]] bool tab_visible(int handle) noexcept;

// The queue of what users did: at most one entry per node - the last value, `committed` if any of the merged
// events was - in the order nodes first changed. Kept while the game is not up (lua_sched only drains it in
// `ready`); an entry of a script that is suspended or gone is dropped on the way out.
[[nodiscard]] bool has_events() noexcept;
bool next_event(int& handle, double& value, bool& committed) noexcept;

// Everything script `owner` declared goes inert and leaves the menu. Freed at the next end_frame().
void drop_owner(int owner) noexcept;

// The overlay frame, from lua_host::draw_frame: begin_frame() republishes the list of tabs when something changed,
// end_frame() frees what was dropped.
void begin_frame() noexcept;
void end_frame() noexcept;

// Shutdown: every tab gone, the menu's list emptied.
void clear() noexcept;
} // namespace tw::lua::tabs
