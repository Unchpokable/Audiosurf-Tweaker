#pragma once

// Script tabs in the overlay menu: tw.ui (Docs/Internal/script-tabs.md §3.6). The extern "C" half the
// prelude binds (prelude/ui.lua); every one of them is a thin pass into lua_tabs, which owns the forms, the
// handles and the event queue. See api_channels.hxx for why everything here looks like C.
//
// A node handle is an int the prelude keeps in its handle tables. A handle that is not live - refused, or of a
// script that has since been unloaded - is not an error anywhere here: reads answer NaN, writes do nothing.
namespace tw::lua::api
{
extern "C" {
// A new tab for script `owner`. Returns its handle, or -1. Called only inside the registration window, which the
// prelude enforces.
int tw_ui_tab(int owner, const char* label) noexcept;

// A new node on `parent` (a tab or a group handle) of `kind` - tw::ui::form::kind, the same numbers as the
// prelude's K_* kinds. Returns its handle, or -1, with the reason in the script's diagnostics.
int tw_ui_add(int parent, int kind, const char* label) noexcept;

// slider, number: the range. choice: the options. list: the rows - `subtext` may be null, and so may any entry
// of it - and the list's height in pixels.
void tw_ui_range(int node, double min_value, double max_value) noexcept;
void tw_ui_options(int node, const char* const* labels, int count) noexcept;
void tw_ui_items(int node, const char* const* text, const char* const* subtext, int count) noexcept;
void tw_ui_height(int node, float height) noexcept;

// The control's value, as a double whatever its kind (ui/form): a toggle 0/1, a choice or a list its 0-based
// index (-1 for no row), a colour packed IM_COL32. NaN for a handle that is not a live control.
double tw_ui_get(int node) noexcept;

// The script's write. Never an event; loses against the user's gesture in progress.
void tw_ui_set(int node, double value) noexcept;

// `which` is lua_tabs::text_slot: 0 the label (a text node's paragraph, a tab's label), 1 the tooltip.
void tw_ui_text(int node, int which, const char* text) noexcept;

// 1 on, 0 off, -1 leaves it as it is.
void tw_ui_flags(int node, int enabled, int visible) noexcept;

// Whether a tab was on screen this frame or the last.
int tw_ui_tab_visible(int node) noexcept;

// For the prelude's dispatch_ui: the next thing a user did - the node, its value, whether the gesture is over.
// 0 when there is nothing left.
int tw_ui_next_event(int* node, double* value, int* committed) noexcept;
}
} // namespace tw::lua::api
