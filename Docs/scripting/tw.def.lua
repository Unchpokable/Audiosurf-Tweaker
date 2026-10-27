---@meta
-- The Audiosurf Tweaker scripting API (tw.*), for Lua Language Server. API 4.
--
-- Generated from Docs/scripting/api.json and Docs/scripting/api-reference.md; do not edit.
-- In a project made by `twllink --new` this is already wired up (.luarc.json). Elsewhere, add the
-- folder holding this file to Lua.workspace.library.

---@class tw.ArrayHandle
local ArrayHandle = {}

---The value at row `index`. Number for `tw.array`, three numbers for `tw.array_vec`, `nil` if either
---channel is unresolved. The game's cursor is saved and restored around the read.
---
---`array:get(index)`
function ArrayHandle:get(index) end

---How many rows the underlying table has; valid indices are `0 .. rows()-1`. `nil` when the column
---cannot be resolved or its table is not connected.
---
---`array:rows()` → number or nil
function ArrayHandle:rows() end

---Writes one row. Same cursor handling as `get`.
---
---`array:set(index, value)` → boolean
function ArrayHandle:set(index, value) end

---@class tw.ButtonHandle
local ButtonHandle = {}

---Switches the control (or a whole group) on, or off with `false`: a switched-off control shows its
---value and takes no input.
---
---`control:enable(on)` → control
---@return tw.ButtonHandle
function ButtonHandle:enable(on) end

---Changes the label - a control's, a group's title, a paragraph's text.
---
---`control:label(text)` → control
---@return tw.ButtonHandle
function ButtonHandle:label(text) end

---`fn()` on every click, in the same overlay frame. A registration, like `on_change`.
---
---`button:on_click(fn)` → button
---@return tw.ButtonHandle
function ButtonHandle:on_click(fn) end

---Shows the control (or a whole group), or hides it with `false`: a hidden one takes no room.
---
---`control:show(on)` → control
---@return tw.ButtonHandle
function ButtonHandle:show(on) end

---Changes the tooltip shown when the pointer rests on the row; `nil` or `""` for none.
---
---`control:tooltip(text)` → control
---@return tw.ButtonHandle
function ButtonHandle:tooltip(text) end

---@class tw.ChoiceHandle
local ChoiceHandle = {}

---Switches the control (or a whole group) on, or off with `false`: a switched-off control shows its
---value and takes no input.
---
---`control:enable(on)` → control
---@return tw.ChoiceHandle
function ChoiceHandle:enable(on) end

---The selected option, as `{ text = "Right", index = 2 }`, indices from 1. One table per option, the
---same table on every call — compare `.text` or `.index`, and do not change it.
---
---`choice:get()` → { text, index }
function ChoiceHandle:get() end

---Changes the label - a control's, a group's title, a paragraph's text.
---
---`control:label(text)` → control
---@return tw.ChoiceHandle
function ChoiceHandle:label(text) end

---`fn(value, committed)` whenever the player changes the control — `value` as `:get()` returns it,
---`committed` true once the change is final (always, except while a slider or a colour is still being
---dragged). Runs in the same overlay frame, before `on_frame`, and only once the game has loaded;
---several changes before it can run come as one, with the last value. A registration: body or
---`tw.on_ready` only.
---
---`control:on_change(fn)` → control
---@return tw.ChoiceHandle
function ChoiceHandle:on_change(fn) end

---Replaces the options. The selection keeps its number, clamped into the new count; option tables you
---held from before are no longer the choice's.
---
---`choice:options(labels)` → choice
---@return tw.ChoiceHandle
function ChoiceHandle:options(labels) end

---Selects an option by its text or its number. One that is not there leaves the choice as it was, with
---a warning in the Scripts tab.
---
---`choice:set(option)` → choice
---@return tw.ChoiceHandle
function ChoiceHandle:set(option) end

---Shows the control (or a whole group), or hides it with `false`: a hidden one takes no room.
---
---`control:show(on)` → control
---@return tw.ChoiceHandle
function ChoiceHandle:show(on) end

---Changes the tooltip shown when the pointer rests on the row; `nil` or `""` for none.
---
---`control:tooltip(text)` → control
---@return tw.ChoiceHandle
function ChoiceHandle:tooltip(text) end

---@class tw.ControlHandle
local ControlHandle = {}

---Switches the control (or a whole group) on, or off with `false`: a switched-off control shows its
---value and takes no input.
---
---`control:enable(on)` → control
---@return tw.ControlHandle
function ControlHandle:enable(on) end

---The control's current value: `true`/`false` for a toggle, a number for a slider or a number, a
---packed colour for a colour. Cheap enough for every frame, and available before the game has loaded.
---A handle that was refused, or whose script has been unloaded, answers with its initial value.
---
---`control:get()` → value
function ControlHandle:get() end

---Changes the label - a control's, a group's title, a paragraph's text.
---
---`control:label(text)` → control
---@return tw.ControlHandle
function ControlHandle:label(text) end

---`fn(value, committed)` whenever the player changes the control — `value` as `:get()` returns it,
---`committed` true once the change is final (always, except while a slider or a colour is still being
---dragged). Runs in the same overlay frame, before `on_frame`, and only once the game has loaded;
---several changes before it can run come as one, with the last value. A registration: body or
---`tw.on_ready` only.
---
---`control:on_change(fn)` → control
---@return tw.ControlHandle
function ControlHandle:on_change(fn) end

---Sets the value from your script. Clamped and rounded like the initial value; nothing happens when it
---is the same; **never** calls `on_change`. Ignored while the player is dragging that control — the
---player wins.
---
---`control:set(value)` → control
---@return tw.ControlHandle
function ControlHandle:set(value) end

---Shows the control (or a whole group), or hides it with `false`: a hidden one takes no room.
---
---`control:show(on)` → control
---@return tw.ControlHandle
function ControlHandle:show(on) end

---Changes the tooltip shown when the pointer rests on the row; `nil` or `""` for none.
---
---`control:tooltip(text)` → control
---@return tw.ControlHandle
function ControlHandle:tooltip(text) end

---@class tw.FloatHandle
local FloatHandle = {}

---Whether this handle has **given up**, which is a different thing from not having resolved yet.
---
---`handle:dead_end()` → boolean
function FloatHandle:dead_end() end

---- float → number, or `nil`
---- text → string, or `nil`
---- vector → three numbers, or `nil`
---- matrix → sixteen numbers, row-major (`_11 _12 _13 _14 _21 … _44`), or `nil`
---
---`handle:get()`
function FloatHandle:get() end

---Whether the game evaluated this channel **in the current frame**: `true`, `false`, or `nil` when
---that cannot be known — some channels are never cached by the engine, and for those nothing records
---when they were last computed.
---
---`handle:live()` → boolean or nil
function FloatHandle:live() end

---Writes a number. Returns `false` if the channel could not be resolved. See
---[Limits](limits.md#writing-to-the-game) before using this.
---
---`handle:set(value)` → boolean
function FloatHandle:set(value) end

---Whether this handle has resolved yet. Does not attempt a resolve.
---
---`handle:valid()` → boolean
function FloatHandle:valid() end

---@class tw.GroupHandle
local GroupHandle = {}

---A button labelled `opts.label`. It has no value; clicks go to
---[`button:on_click`](#buttonon_clickfn--button).
---
---`parent:button(opts)` → button
---@return tw.ButtonHandle
function GroupHandle:button(opts) end

---One of several options, as a strip of chips. `opts.options` — the options, a list of strings
---(required). `opts.value` — the initial option, by its text or its number; default the first, and the
---first, with a warning, for one that is not there.
---
---`parent:choice(opts)` → choice
---@return tw.ChoiceHandle
function GroupHandle:choice(opts) end

---A colour, with a picker. Its value is packed the way [`tw.rgb`](#twrgbr-g-b-a--colour) returns
---colours and every `tw.hud.*` call takes them. `opts.value` — default white.
---
---`parent:color(opts)` → control
---@return tw.ControlHandle
function GroupHandle:color(opts) end

---Switches the control (or a whole group) on, or off with `false`: a switched-off control shows its
---value and takes no input.
---
---`control:enable(on)` → control
---@return tw.GroupHandle
function GroupHandle:enable(on) end

---A framed section across the page, titled by `opts.label` (or the bare string). Takes every
---constructor a tab takes; groups nest two deep at most.
---
---`parent:group(opts)` → group
---@return tw.GroupHandle
function GroupHandle:group(opts) end

---Changes the label - a control's, a group's title, a paragraph's text.
---
---`control:label(text)` → control
---@return tw.GroupHandle
function GroupHandle:label(text) end

---A list of rows, one of which the player can select. `opts.items` — the rows: strings, or `{ text =
---..., sub = ... }` for a row with a smaller second line. `opts.value` — the selected row, from 1;
---none by default. `opts.height` — in pixels, 160 by default.
---
---`parent:list(opts)` → list
---@return tw.ListHandle
function GroupHandle:list(opts) end

---A whole number in a range. `opts.min` (0), `opts.max` (100), `opts.value` (default `min`, rounded
---and clamped).
---
---`parent:number(opts)` → control
---@return tw.ControlHandle
function GroupHandle:number(opts) end

---A line across the page.
---
---`parent:separator()` → separator
---@return tw.SeparatorHandle
function GroupHandle:separator() end

---Shows the control (or a whole group), or hides it with `false`: a hidden one takes no room.
---
---`control:show(on)` → control
---@return tw.GroupHandle
function GroupHandle:show(on) end

---A number in a range, dragged. `opts.min` (0), `opts.max` (1), `opts.value` (default `min`, clamped
---into the range). While it is being dragged, `on_change` hears the values with `committed = false`.
---
---`parent:slider(opts)` → control
---@return tw.ControlHandle
function GroupHandle:slider(opts) end

---A paragraph of text, wrapped to the page, in the overlay's secondary text colour. `opts.text`, or
---the text as a bare string.
---
---`parent:text(opts)` → paragraph
---@return tw.ParagraphHandle
function GroupHandle:text(opts) end

---A switch; its value is `true` or `false`. `opts.value` — the initial value, default `false`.
---
---`parent:toggle(opts)` → control
---@return tw.ControlHandle
function GroupHandle:toggle(opts) end

---Changes the tooltip shown when the pointer rests on the row; `nil` or `""` for none.
---
---`control:tooltip(text)` → control
---@return tw.GroupHandle
function GroupHandle:tooltip(text) end

---@class tw.ListHandle
local ListHandle = {}

---Switches the control (or a whole group) on, or off with `false`: a switched-off control shows its
---value and takes no input.
---
---`control:enable(on)` → control
---@return tw.ListHandle
function ListHandle:enable(on) end

---The selected row's number, from 1, or `nil` when no row is selected.
---
---`list:get()` → number or nil
function ListHandle:get() end

---Replaces the rows - strings, or `{ text = ..., sub = ... }`. A selection past the new end is
---dropped.
---
---`list:items(rows)` → list
---@return tw.ListHandle
function ListHandle:items(rows) end

---Changes the label - a control's, a group's title, a paragraph's text.
---
---`control:label(text)` → control
---@return tw.ListHandle
function ListHandle:label(text) end

---`fn(value, committed)` whenever the player changes the control — `value` as `:get()` returns it,
---`committed` true once the change is final (always, except while a slider or a colour is still being
---dragged). Runs in the same overlay frame, before `on_frame`, and only once the game has loaded;
---several changes before it can run come as one, with the last value. A registration: body or
---`tw.on_ready` only.
---
---`control:on_change(fn)` → control
---@return tw.ListHandle
function ListHandle:on_change(fn) end

---Selects a row by its number, or none with `nil`.
---
---`list:set(row)` → list
---@return tw.ListHandle
function ListHandle:set(row) end

---Shows the control (or a whole group), or hides it with `false`: a hidden one takes no room.
---
---`control:show(on)` → control
---@return tw.ListHandle
function ListHandle:show(on) end

---Changes the tooltip shown when the pointer rests on the row; `nil` or `""` for none.
---
---`control:tooltip(text)` → control
---@return tw.ListHandle
function ListHandle:tooltip(text) end

---@class tw.MatrixHandle
local MatrixHandle = {}

---Whether this handle has **given up**, which is a different thing from not having resolved yet.
---
---`handle:dead_end()` → boolean
function MatrixHandle:dead_end() end

---- float → number, or `nil`
---- text → string, or `nil`
---- vector → three numbers, or `nil`
---- matrix → sixteen numbers, row-major (`_11 _12 _13 _14 _21 … _44`), or `nil`
---
---`handle:get()`
function MatrixHandle:get() end

---Whether the game evaluated this channel **in the current frame**: `true`, `false`, or `nil` when
---that cannot be known — some channels are never cached by the engine, and for those nothing records
---when they were last computed.
---
---`handle:live()` → boolean or nil
function MatrixHandle:live() end

---Writes all sixteen elements, either as sixteen arguments or as one table of sixteen numbers in the
---same row-major order. Returns `false` if it could not be resolved. Subject to the same rules as any
---other write — see [Limits](limits.md#writing-to-the-game).
---
---`handle:set(m11, m12, …, m44)` → boolean
---`handle:set(table)` → boolean
function MatrixHandle:set(first, ...) end

---Whether this handle has resolved yet. Does not attempt a resolve.
---
---`handle:valid()` → boolean
function MatrixHandle:valid() end

---@class tw.MuteHandle
local MuteHandle = {}

---Whether it is registered *and* currently suppressing.
---
---`mute:active()` → boolean
function MuteHandle:active() end

---Turn suppression on or off without re-hooking. All return the handle, so they chain.
---
---`mute:off()`
function MuteHandle:off() end

---Turn suppression on or off without re-hooking. All return the handle, so they chain.
---
---`mute:on()`
function MuteHandle:on() end

---Turn suppression on or off without re-hooking. All return the handle, so they chain.
---
---`mute:set(bool)`
function MuteHandle:set(on) end

---@class tw.ParagraphHandle
local ParagraphHandle = {}

---Switches the control (or a whole group) on, or off with `false`: a switched-off control shows its
---value and takes no input.
---
---`control:enable(on)` → control
---@return tw.ParagraphHandle
function ParagraphHandle:enable(on) end

---Changes the label - a control's, a group's title, a paragraph's text.
---
---`control:label(text)` → control
---@return tw.ParagraphHandle
function ParagraphHandle:label(text) end

---Replaces the paragraph's text. Setting the same text every frame costs a comparison.
---
---`paragraph:set(text)` → paragraph
---@return tw.ParagraphHandle
function ParagraphHandle:set(text) end

---Shows the control (or a whole group), or hides it with `false`: a hidden one takes no room.
---
---`control:show(on)` → control
---@return tw.ParagraphHandle
function ParagraphHandle:show(on) end

---Changes the tooltip shown when the pointer rests on the row; `nil` or `""` for none.
---
---`control:tooltip(text)` → control
---@return tw.ParagraphHandle
function ParagraphHandle:tooltip(text) end

---@class tw.SeparatorHandle
local SeparatorHandle = {}

---Shows the control (or a whole group), or hides it with `false`: a hidden one takes no room.
---
---`control:show(on)` → control
---@return tw.SeparatorHandle
function SeparatorHandle:show(on) end

---@class tw.TabHandle
local TabHandle = {}

---A button labelled `opts.label`. It has no value; clicks go to
---[`button:on_click`](#buttonon_clickfn--button).
---
---`parent:button(opts)` → button
---@return tw.ButtonHandle
function TabHandle:button(opts) end

---One of several options, as a strip of chips. `opts.options` — the options, a list of strings
---(required). `opts.value` — the initial option, by its text or its number; default the first, and the
---first, with a warning, for one that is not there.
---
---`parent:choice(opts)` → choice
---@return tw.ChoiceHandle
function TabHandle:choice(opts) end

---A colour, with a picker. Its value is packed the way [`tw.rgb`](#twrgbr-g-b-a--colour) returns
---colours and every `tw.hud.*` call takes them. `opts.value` — default white.
---
---`parent:color(opts)` → control
---@return tw.ControlHandle
function TabHandle:color(opts) end

---A framed section across the page, titled by `opts.label` (or the bare string). Takes every
---constructor a tab takes; groups nest two deep at most.
---
---`parent:group(opts)` → group
---@return tw.GroupHandle
function TabHandle:group(opts) end

---Changes the tab's label, from the next frame. Clashes are worked out again.
---
---`tab:label(text)` → tab
---@return tw.TabHandle
function TabHandle:label(text) end

---A list of rows, one of which the player can select. `opts.items` — the rows: strings, or `{ text =
---..., sub = ... }` for a row with a smaller second line. `opts.value` — the selected row, from 1;
---none by default. `opts.height` — in pixels, 160 by default.
---
---`parent:list(opts)` → list
---@return tw.ListHandle
function TabHandle:list(opts) end

---A whole number in a range. `opts.min` (0), `opts.max` (100), `opts.value` (default `min`, rounded
---and clamped).
---
---`parent:number(opts)` → control
---@return tw.ControlHandle
function TabHandle:number(opts) end

---A line across the page.
---
---`parent:separator()` → separator
---@return tw.SeparatorHandle
function TabHandle:separator() end

---A number in a range, dragged. `opts.min` (0), `opts.max` (1), `opts.value` (default `min`, clamped
---into the range). While it is being dragged, `on_change` hears the values with `committed = false`.
---
---`parent:slider(opts)` → control
---@return tw.ControlHandle
function TabHandle:slider(opts) end

---A paragraph of text, wrapped to the page, in the overlay's secondary text colour. `opts.text`, or
---the text as a bare string.
---
---`parent:text(opts)` → paragraph
---@return tw.ParagraphHandle
function TabHandle:text(opts) end

---A switch; its value is `true` or `false`. `opts.value` — the initial value, default `false`.
---
---`parent:toggle(opts)` → control
---@return tw.ControlHandle
function TabHandle:toggle(opts) end

---Whether the tab was on screen this frame or the last. For deciding whether a status text is worth
---building; reading values never needs it.
---
---`tab:visible()` → boolean
function TabHandle:visible() end

---@class tw.TextHandle
local TextHandle = {}

---Whether this handle has **given up**, which is a different thing from not having resolved yet.
---
---`handle:dead_end()` → boolean
function TextHandle:dead_end() end

---- float → number, or `nil`
---- text → string, or `nil`
---- vector → three numbers, or `nil`
---- matrix → sixteen numbers, row-major (`_11 _12 _13 _14 _21 … _44`), or `nil`
---
---`handle:get()`
function TextHandle:get() end

---Whether the game evaluated this channel **in the current frame**: `true`, `false`, or `nil` when
---that cannot be known — some channels are never cached by the engine, and for those nothing records
---when they were last computed.
---
---`handle:live()` → boolean or nil
function TextHandle:live() end

---Whether this handle has resolved yet. Does not attempt a resolve.
---
---`handle:valid()` → boolean
function TextHandle:valid() end

---@class tw.VectorArrayHandle
local VectorArrayHandle = {}

---The value at row `index`. Number for `tw.array`, three numbers for `tw.array_vec`, `nil` if either
---channel is unresolved. The game's cursor is saved and restored around the read.
---
---`array:get(index)`
function VectorArrayHandle:get(index) end

---How many rows the underlying table has; valid indices are `0 .. rows()-1`. `nil` when the column
---cannot be resolved or its table is not connected.
---
---`array:rows()` → number or nil
function VectorArrayHandle:rows() end

---Writes one row. Same cursor handling as `get`.
---
---`array:set(index, x, y, z)` → boolean
function VectorArrayHandle:set(index, x, y, z) end

---@class tw.VectorHandle
local VectorHandle = {}

---Whether this handle has **given up**, which is a different thing from not having resolved yet.
---
---`handle:dead_end()` → boolean
function VectorHandle:dead_end() end

---- float → number, or `nil`
---- text → string, or `nil`
---- vector → three numbers, or `nil`
---- matrix → sixteen numbers, row-major (`_11 _12 _13 _14 _21 … _44`), or `nil`
---
---`handle:get()`
function VectorHandle:get() end

---Whether the game evaluated this channel **in the current frame**: `true`, `false`, or `nil` when
---that cannot be known — some channels are never cached by the engine, and for those nothing records
---when they were last computed.
---
---`handle:live()` → boolean or nil
function VectorHandle:live() end

---Writes a vector channel. Returns `false` if it could not be resolved.
---
---`handle:set(x, y, z)` → boolean
function VectorHandle:set(x, y, z) end

---Whether this handle has resolved yet. Does not attempt a resolve.
---
---`handle:valid()` → boolean
function VectorHandle:valid() end

---@class tw
tw = {}

---The same colour with alpha set to exactly `a` (0..1).
---
---`tw.alpha(colour, a)` → colour
function tw.alpha(colour, a) end

---The version of this API: `4` today (API 4 added
---[`tw.hud.polyline`](#twhudpolylinexs-ys-colour-thickness-closed) and
---[`tw.hud.gradient_polyline`](#twhudgradient_polylinexs-ys-colours-thickness-closed), API 3 [menu
---tabs](#menu-tabs) and [storage](#storage)), raised whenever a name is added (names are never taken
---away). A script can use it to adapt to an older Tweaker. A script built with `twllink` also records
---the version it was written against in its header (`-- @api`), and a Tweaker older than that refuses
---to run it rather than let it fail on a missing name — see [Projects § Checks at build
---time](authoring.md#checks-at-build-time).
---
---`tw.api_version` → number
---@type number
tw.api_version = 0

---An `Array Value` column plus its cursor channel. Both are numeric.
---
---Writes one row. Same cursor handling as `get`.
---
---`tw.array(group, column, cursor)` → array handle
---`tw.array` → boolean
---@return tw.ArrayHandle
function tw.array(group, column, cursor) end

---An `Array Vector` column plus its cursor. The column is a vector channel, the cursor numeric.
---
---Writes one row. Same cursor handling as `get`.
---
---`tw.array_vec(group, column, cursor)` → array handle
---`tw.array_vec` → boolean
---@return tw.VectorArrayHandle
function tw.array_vec(group, column, cursor) end

---An alias for `tw.ready()`, kept for scripts written before it. Writes used to have a gate of their
---own; they do not any more, because "has the game finished loading" turned out to be one question
---with one answer.
---
---`tw.can_write()` → boolean
function tw.can_write() end

---The old name of `tw.float_ch`, kept so scripts written before the typed accessors still run. New
---code should say `tw.float_ch`.
---
---`tw.channel(group, name)` → handle
---@deprecated
---@return tw.FloatHandle
function tw.channel(group, name) end

---Overlay frames drawn since the VM started, for the rare thing that really is about drawing rate.
---Almost always the wrong number to throttle on; `tw.frame` is the right one.
---
---`tw.draw_frame_count`
---@type number
tw.draw_frame_count = 0

---Seconds since the previous frame. Anything that moves over time should be driven by this rather than
---by a fixed step per frame — the game does not run at a fixed frame rate, and a constant per-frame
---step animates at whatever speed the machine happens to reach.
---
---`tw.dt()` → number
function tw.dt() end

---One of the bundled easing curves evaluated at `t`. `t` is clamped to `0..1` and the result is in
---`0..1`. A pure function: your script keeps the progress, this shapes it.
---
---`tw.ease(curve, t)` → number
function tw.ease(curve, t) end

---Every valid name for `tw.ease`, sorted. Currently: `linear`, `quadIn/Out/InOut`,
---`cubicIn/Out/InOut`, `sineIn/Out/InOut`, `expoIn/Out/InOut`, `backIn/Out/InOut`, `elasticOut`,
---`bounceOut`.
---
---`tw.ease_names()` → table
function tw.ease_names() end

---Whether the game's channel graph is **reachable** — a weaker thing than `tw.ready()`, and a
---different question. It goes true about a second into the process, as soon as the game has run its
---first frame, which is long before the game has finished loading.
---
---`tw.engine_ready()` → boolean
function tw.engine_ready() end

---Messages for the script's author, filed in the script's row in the Scripts tab and in the [script
---log](getting-started.md#the-script-log) — which, unlike `tw.log`, exist in a normal install. All
---three remember where they were said from (`myhud.lua:88`), and saying the same thing from the same
---place again only increases a counter next to it: calling one of these every frame is one line, not
---thousands.
---
---`tw.error(message)`
function tw.error(msg) end

---The same colour with its existing alpha *scaled* by `k` (0..1). This is the one for fading a whole
---widget.
---
---`tw.fade(colour, k)` → colour
function tw.fade(colour, k) end

---Create a handle for a numeric, text, vector or matrix channel. Cheap; never fails; does not touch
---the game. Resolution happens on first use and is retried until it succeeds.
---
---`tw.float_ch(group, name)` → handle
---@return tw.FloatHandle
function tw.float_ch(group, name) end

---The **engine** frame number: one per evaluation of the game's channel graph. Read-only in practice,
---and monotonic. Useful for throttling:
---
---`tw.frame`
---@type number
tw.frame = 0

---Every channel group currently loaded, as `"<pool name> | <file>"`. The answer to "why will my group
---not resolve".
---
---`tw.groups()` → table of strings
function tw.groups() end

---@class tw.hud
tw.hud = {}

---Writes to the plugin's debug log. **Stripped from release builds**, so it is a development tool only
---— and it does not go to the [script log](getting-started.md#the-script-log) either. For a message
---you need to see in a normal install, use `tw.warn`.
---
---`tw.log(message)`
function tw.log(msg) end

---Create a handle for a numeric, text, vector or matrix channel. Cheap; never fails; does not touch
---the game. Resolution happens on first use and is retried until it succeeds.
---
---`tw.matrix_ch(group, name)` → handle
---@return tw.MatrixHandle
function tw.matrix_ch(group, name) end

---Suppresses a channel: the game keeps calling it and it does nothing. Starts active.
---
---`tw.mute(group, name)` → mute handle
---@return tw.MuteHandle
function tw.mute(group, name) end

---Raises a toast in the overlay's notification strip. For events, not for state — do not call it every
---frame. Capped per script: a handful a minute, then one line saying so, and the rest are collected in
---the script's messages in the Scripts tab.
---
---`tw.notify(message)`
function tw.notify(msg) end

---Runs `fn` when that channel is called by the game. `when` is `"after"` (default) or `"before"`.
---
---`tw.on_call(group, name, when, fn)`
---`tw.on_call(group, name, fn)`
function tw.on_call(group, name, when, fn) end

---Registers `fn` to run once per drawn overlay frame, for as long as the script is enabled. Call at
---the top level. Several handlers per script are allowed; they run in registration order.
---
---`tw.on_frame(fn)`
---`tw.on_frame(fn, { before_ready = true })`
function tw.on_frame(fn, opts) end

---Registers `fn(loaded)` to run when that channel group appears or disappears. `name` is a pool name
---(`"Renderer"`) or a bare file name (`"Puzzle"`), the same two spellings every other call accepts.
---
---`tw.on_group(name, fn)`
function tw.on_group(name, fn) end

---The same, immediately **after** the graph has been evaluated. This is where the results of the frame
---that just happened are readable.
---
---`tw.on_post_tick(fn)`
function tw.on_post_tick(fn) end

---Registers `fn` to run **once**, on the first frame where everything is allowed: the graph is up,
---channels resolve, writes land.
---
---`tw.on_ready(fn)`
function tw.on_ready(fn) end

---Registers `fn(state)` to run on every change of [`tw.state()`](#twstate--string), with the new state
---as a string. Unlike everything else here, **this runs in all states**, including while the game is
---still loading — it is how the layer reports what it is doing.
---
---`tw.on_state(fn)`
function tw.on_state(fn) end

---Registers `fn` to run once per engine frame, immediately **before** the game evaluates its channel
---graph. Call at the top level; several handlers per script are allowed and run in registration order.
---
---`tw.on_tick(fn)`
function tw.on_tick(fn) end

---Registers `fn` to run when the script is switched off or reloaded — before its hooks are taken out,
---so it can still reach the channels it held. The place to put back whatever the script changed.
---
---`tw.on_unload(fn)`
function tw.on_unload(fn) end

---Messages for the script's author, filed in the script's row in the Scripts tab and in the [script
---log](getting-started.md#the-script-log) — which, unlike `tw.log`, exist in a normal install. All
---three remember where they were said from (`myhud.lua:88`), and saying the same thing from the same
---place again only increases a counter next to it: calling one of these every frame is one line, not
---thousands.
---
---`tw.pending(message)`
function tw.pending(msg) end

---True in `"ready"` and `"busy"`: the graph is up and reading, writing and hooking all work.
---
---`tw.ready()` → boolean
function tw.ready() end

---Packs 0..1 floats. `a` defaults to 1. Designed to take a vector channel's output directly:
---`tw.rgb(colour_ch:get())`.
---
---`tw.rgb(r, g, b, a)` → colour
function tw.rgb(r, g, b, a) end

---Where the game is, as one of:
---
---`tw.state()` → string
function tw.state() end

---@class tw.storage
tw.storage = {}

---Create a handle for a numeric, text, vector or matrix channel. Cheap; never fails; does not touch
---the game. Resolution happens on first use and is retried until it succeeds.
---
---`tw.string_ch(group, name)` → handle
---@return tw.TextHandle
function tw.string_ch(group, name) end

---A colour from the overlay's live palette. Read at call time, so it follows theme changes. An unknown
---name raises an error.
---
---`tw.theme(name)` → colour
function tw.theme(name) end

---Every valid name for `tw.theme`, sorted.
---
---`tw.theme_names()` → table
function tw.theme_names() end

---@class tw.ui
tw.ui = {}

---Create a handle for a numeric, text, vector or matrix channel. Cheap; never fails; does not touch
---the game. Resolution happens on first use and is retried until it succeeds.
---
---`tw.vector_ch(group, name)` → handle
---@return tw.VectorHandle
function tw.vector_ch(group, name) end

---Messages for the script's author, filed in the script's row in the Scripts tab and in the [script
---log](getting-started.md#the-script-log) — which, unlike `tw.log`, exist in a normal install. All
---three remember where they were said from (`myhud.lua:88`), and saying the same thing from the same
---place again only increases a counter next to it: calling one of these every frame is one line, not
---thousands.
---
---`tw.warn(message)`
function tw.warn(msg) end

---Takes a box for this frame without drawing in it, as if something were drawn there. For a widget
---that fades out, blinks, or is not drawn every frame: without it, the scripts below move into its
---place on the frames it is not drawn and back out after. Call it every frame the box should stay
---taken, from `on_frame` — it does nothing anywhere else.
---
---`tw.hud.claim(x0, y0, x1, y1)`
function tw.hud.claim(x0, y0, x1, y1) end

---A table of corner masks for `tw.hud.rect`: `none`, `top_left`, `top_right`, `bottom_left`,
---`bottom_right`, `top`, `bottom`, `left`, `right`, `all`. They are plain bits, so they add:
---`tw.hud.corners.top_left + tw.hud.corners.bottom_right`.
---
---`tw.hud.corners`
tw.hud.corners = { all = 0, bottom = 0, bottom_left = 0, bottom_right = 0, left = 0, none = 0, right = 0, top = 0, top_left = 0, top_right = 0 }

---The overlay's default text height, in pixels. Scale layouts off this.
---
---`tw.hud.font_size()` → number
function tw.hud.font_size() end

---Every face name `tw.hud.text` accepts, sorted. Currently `regular` and `semibold`. Faces are
---weights, not sizes — `size` and `font` are independent. An unknown name raises an error.
---
---`tw.hud.fonts()` → table
function tw.hud.fonts() end

---A soft glow around a rounded rectangle, in the overlay's own style. `strength` is `0..1` and
---defaults to 1.
---
---`tw.hud.glow_rect(x0, y0, x1, y1, colour, rounding, strength)`
function tw.hud.glow_rect(x0, y0, x1, y1, color, rounding, strength) end

---Text with a glow behind it. Draws the text as well, unlike the rect version — the glow is offset
---copies of the same glyphs, so splitting it in two would rasterize them twice. `glow` defaults to the
---text colour.
---
---`tw.hud.glow_text(x, y, text, colour, glow, size, font, strength)`
function tw.hud.glow_text(x, y, text, color, glow, size, font, strength) end

---`tw.hud.polyline` with a colour per point: `colours[i]` belongs to point `i`, and the colour blends
---between neighbouring points along each segment (API 4). `colours` is required and has as many
---entries as there are points. Same limits and errors as `tw.hud.polyline`.
---
---`tw.hud.gradient_polyline(xs, ys, colours, thickness, closed)`
function tw.hud.gradient_polyline(xs, ys, colours, thickness, closed) end

---A rectangle filled with a two-stop linear gradient: `from` at the left edge and `to` at the right,
---or top and bottom when `vertical` is true. Both colours carry their own alpha, so a backdrop that
---fades out to nothing is the same call as one colour fading into another.
---
---`tw.hud.gradient_rect(x0, y0, x1, y1, from, to, vertical)`
function tw.hud.gradient_rect(x0, y0, x1, y1, from, to, vertical) end

---One of the plugin's built-in SVG icons, in a square box of `size` pixels. `name` is a bare stem —
---`"feat_stealth"` — and reaches `icons/feat_stealth.svg` inside the plugin. Icons are monochrome and
---take their colour from `colour`.
---
---`tw.hud.icon(name, x, y, size, colour)`
function tw.hud.icon(name, x, y, size, colour) end

---`thickness` defaults to 1.
---
---`tw.hud.line(x0, y0, x1, y1, colour, thickness)`
function tw.hud.line(x0, y0, x1, y1, color, thickness) end

---What that text would occupy. Measured by the same engine that draws it. **Pass the same `font` you
---will draw with** — measuring one face and drawing another is off by enough to be visible in anything
---centred or right-aligned, and nothing can catch that for you.
---
---`tw.hud.measure(text, size, font)` → width, height
function tw.hud.measure(text, size, font) end

---What is already on screen this frame, as a list of `{ x0, y0, x1, y1, by }`: first the overlay's
---chrome (`by` is `"notefeed"`, `"pins"`, `"watermark"` or `"menu"`, only those on screen), then the
---boxes of everything the scripts above this one drew or claimed (`by` is the script's file name).
---Pieces of one script closer than a few pixels are merged into one box, so a block of text lines is
---one box. Never this script's own. A new list on every call — call it once per frame.
---
---`tw.hud.occupied()` → list
function tw.hud.occupied() end

---The free spot nearest to `corner` for a `w` × `h` box, and takes it as if drawn. `corner` is
---`"top_left"` (default), `"top_right"`, `"bottom_left"` or `"bottom_right"`; `margin` (default 8) is
---kept from the screen edges and from everything taken. It stays clear of the overlay's chrome —
---except the menu, which comes and goes — of what the scripts above drew, and of what this script
---itself drew or placed earlier this frame, so two boxes placed at one corner stack. `nil` when
---nothing fits. An unknown corner raises an error. Only from `on_frame`.
---
---`tw.hud.place(w, h, corner, margin)` → x0, y0 or nil
function tw.hud.place(w, h, corner, margin) end

---One line through every point (`xs[i]`, `ys[i]`) in order, as a single stroke with joined corners
---(API 4). `xs` and `ys` are arrays of numbers of the same length, at most 4096; fewer than two points
---draw nothing. `colour` defaults to opaque white, `thickness` to 1; `closed` joins the last point
---back to the first.
---
---`tw.hud.polyline(xs, ys, colour, thickness, closed)`
function tw.hud.polyline(xs, ys, colour, thickness, closed) end

---`rounding` is the corner radius (default 0). `thickness` ≤ 0 (the default) fills; positive strokes
---an outline of that width. `corners` selects which corners the radius applies to and defaults to all
---of them; see [`tw.hud.corners`](#twhudcorners).
---
---`tw.hud.rect(x0, y0, x1, y1, colour, rounding, thickness, corners)`
function tw.hud.rect(x0, y0, x1, y1, color, rounding, thickness, corners) end

---The viewport minus the overlay's always-on top band. Full width. Coarse: the band is as tall as the
---notification column and spans the whole screen, so it is not for layout — position widgets with
---`tw.hud.place`, or check a position of your own against `tw.hud.occupied` (below and [Drawing §
---Coordinates](drawing.md#coordinates)).
---
---`tw.hud.safe()` → x0, y0, x1, y1
function tw.hud.safe() end

---The viewport, in pixels.
---
---`tw.hud.size()` → width, height
function tw.hud.size() end

---Draws text with its top-left corner at `(x, y)`. `colour` defaults to opaque white, `size` to the
---overlay's own text height, `font` to the default face.
---
---`tw.hud.text(x, y, text, colour, size, font)`
function tw.hud.text(x, y, text, color, size, font) end

---Exact rectangle of one overlay widget this frame. `name` is `"notefeed"`, `"pins"`, `"watermark"` or
---`"menu"`. `nil` when that widget is not on screen. An unknown name raises an error.
---
---`tw.hud.widget(name)` → x0, y0, x1, y1 or nil
function tw.hud.widget(name) end

---Whether the file is there - on disk, or written and not yet saved.
---
---`tw.storage.exists(name)` → boolean
function tw.storage.exists(name) end

---The names of the script's files, sorted.
---
---`tw.storage.list()` → table of strings
function tw.storage.list() end

---The file as Lua values: objects become tables with string keys, arrays sequences from 1, `null` a
---missing key. A file that is not JSON is `nil` and why.
---
---`tw.storage.read_json(name)` → value or nil, reason
function tw.storage.read_json(name) end

---The file's contents, byte for byte. The first read of a file on disk reads it; after that it comes
---from memory.
---
---`tw.storage.read_text(name)` → string or nil, reason
function tw.storage.read_text(name) end

---Deletes the file.
---
---`tw.storage.remove(name)` → true or nil, reason
function tw.storage.remove(name) end

---Writes a table as indented JSON, keys sorted. Refuses, with the reason, what JSON cannot hold: NaN
---and infinities, mixed or non-sequential keys, functions, a table inside itself, nesting deeper than
---64.
---
---`tw.storage.write_json(name, value)` → true or nil, reason
function tw.storage.write_json(name, value) end

---Replaces the file's contents. Returns at once: the plugin writes it to disk within a tenth of a
---second, through a temporary file. Up to 1 MiB a file, 64 files and 8 MiB a script.
---
---`tw.storage.write_text(name, text)` → true or nil, reason
function tw.storage.write_text(name, text) end

---A new tab in the menu, after the overlay's own and after the tabs of scripts that run before yours.
---`opts` is `{ label = ... }`, or the label as a bare string; no label names the tab after your
---script. A label that clashes with another tab's is shown as `<your script's name> - <label>`.
---Declared outside the registration window, it returns an inert tab whose controls are inert too.
---
---`tw.ui.tab(opts)` → tab
---@return tw.TabHandle
function tw.ui.tab(opts) end
