---@meta
-- The Audiosurf Tweaker scripting API (tw.*), for Lua Language Server. API 1.
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

---The version of this API: `1` today, raised whenever a name is added (names are never taken away). A
---script can use it to adapt to an older Tweaker. A script built with `twllink` also records the
---version it was written against in its header (`-- @api`), and a Tweaker older than that refuses to
---run it rather than let it fail on a missing name — see [Projects § Checks at build
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

---Messages for the script's author, filed in the script's row in the Scripts tab — which, unlike the
---log, exists in a normal install. All three remember where they were said from (`myhud.lua:88`), and
---saying the same thing from the same place again only increases a counter next to it: calling one of
---these every frame is one line, not thousands.
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

---Writes to the plugin log. **Stripped from release builds**, so it is a development tool only.
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

---Messages for the script's author, filed in the script's row in the Scripts tab — which, unlike the
---log, exists in a normal install. All three remember where they were said from (`myhud.lua:88`), and
---saying the same thing from the same place again only increases a counter next to it: calling one of
---these every frame is one line, not thousands.
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

---Create a handle for a numeric, text, vector or matrix channel. Cheap; never fails; does not touch
---the game. Resolution happens on first use and is retried until it succeeds.
---
---`tw.vector_ch(group, name)` → handle
---@return tw.VectorHandle
function tw.vector_ch(group, name) end

---Messages for the script's author, filed in the script's row in the Scripts tab — which, unlike the
---log, exists in a normal install. All three remember where they were said from (`myhud.lua:88`), and
---saying the same thing from the same place again only increases a counter next to it: calling one of
---these every frame is one line, not thousands.
---
---`tw.warn(message)`
function tw.warn(msg) end

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

---`rounding` is the corner radius (default 0). `thickness` ≤ 0 (the default) fills; positive strokes
---an outline of that width. `corners` selects which corners the radius applies to and defaults to all
---of them; see [`tw.hud.corners`](#twhudcorners).
---
---`tw.hud.rect(x0, y0, x1, y1, colour, rounding, thickness, corners)`
function tw.hud.rect(x0, y0, x1, y1, color, rounding, thickness, corners) end

---The viewport minus the overlay's always-on top band. Full width.
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
