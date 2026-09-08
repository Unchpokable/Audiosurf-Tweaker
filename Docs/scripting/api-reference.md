# API reference

[← back to the index](../scripting.md)

Everything lives in the global table `tw`. It is already there; there is no `require`.

Throughout: `group` is a group name (string), `name` is a channel name (string) **or** a channel
index (number).

---

## Lifecycle

### `tw.on_frame(fn)`

Registers `fn` to run once per frame, for as long as the script is enabled. Call at the top level.
Several handlers per script are allowed; they run in registration order.

This is the only place drawing works.

### `tw.frame`

Number of frames dispatched since the VM started. Read-only in practice. Useful for throttling:

```lua
if tw.frame % 30 == 0 then refresh_something_expensive() end
```

### `tw.dt()` → number

Seconds since the previous frame. Anything that moves over time should be driven by this rather than
by a fixed step per frame — the game does not run at a fixed frame rate, and a constant per-frame
step animates at whatever speed the machine happens to reach.

### `tw.ease(curve, t)` → number

One of the bundled easing curves evaluated at `t`. `t` is clamped to `0..1` and the result is in
`0..1`. A pure function: your script keeps the progress, this shapes it.

```lua
local t = 0
tw.on_frame(function()
    t = math.min(1, t + tw.dt() / 0.25)      -- 250 ms
    local k = tw.ease("cubicOut", t)
end)
```

There is deliberately no tween object to create or destroy. An unknown curve name raises an error.

### `tw.ease_names()` → table

Every valid name for `tw.ease`, sorted. Currently: `linear`, `quadIn/Out/InOut`,
`cubicIn/Out/InOut`, `sineIn/Out/InOut`, `expoIn/Out/InOut`, `backIn/Out/InOut`, `elasticOut`,
`bounceOut`.

---

## Channels

### `tw.float_ch(group, name)` → handle
### `tw.string_ch(group, name)` → handle
### `tw.vector_ch(group, name)` → handle

Create a handle for a numeric, text or vector channel. Cheap; never fails; does not touch the game.
Resolution happens on first use and is retried until it succeeds.

`tw.channel` is a deprecated alias for `tw.float_ch`.

### `handle:get()`

- float → number, or `nil`
- text → string, or `nil`
- vector → three numbers, or `nil`

`nil` means "not available" — the group is not loaded, the engine is not reachable yet, or the
channel does not exist. Always handle it.

### `handle:set(value)` → boolean *(float handles)*

Writes a number. Returns `false` if the channel could not be resolved. See
[Limits](limits.md#writing-to-the-game) before using this.

### `handle:set(x, y, z)` → boolean *(vector handles)*

Writes a vector channel. Returns `false` if it could not be resolved.

Wider effect than the numeric setter: the engine also writes each component into the numeric channel
wired to that component, where one is wired. See
[Reading and writing the game § Writing a vector](channels.md#writing-a-vector).

### `handle:valid()` → boolean

Whether this handle has resolved yet. Does not attempt a resolve.

### `tw.array(group, column, cursor)` → array handle

An `Array Value` column plus its cursor channel. Both are numeric.

### `tw.array_vec(group, column, cursor)` → array handle

An `Array Vector` column plus its cursor. The column is a vector channel, the cursor numeric.

### `array:get(index)`

The value at row `index`. Number for `tw.array`, three numbers for `tw.array_vec`, `nil` if either
channel is unresolved. The game's cursor is saved and restored around the read.

### `array:set(index, value)` → boolean *(`tw.array`)*
### `array:set(index, x, y, z)` → boolean *(`tw.array_vec`)*

Writes one row. Same cursor handling as `get`.

`false` means the write did not happen, and **"there is no such row" is one of the reasons** — unlike
every other write in this API, an out-of-range index is refused rather than attempted. It has to be:
the engine's write path creates a missing row instead of rejecting it, which would lengthen a table
the rest of the game reads. See [Writing a table row](channels.md#writing-a-table-row).

Subject to the same rules as any other write — see [Limits](limits.md#writing-to-the-game).

### `array:rows()` → number *or* `nil`

How many rows the underlying table has; valid indices are `0 .. rows()-1`. `nil` when the column
cannot be resolved or its table is not connected.

---

## Hooks

### `tw.on_call(group, name, when, fn)`
### `tw.on_call(group, name, fn)`

Runs `fn` when that channel is called by the game. `when` is `"after"` (default) or `"before"`.

`fn` takes no arguments. From a `"before"` handler, returning `false` cancels the game's own handler;
any other return proceeds. Returning `false` from `"after"` does nothing.

Registration is retried until the group is loaded.

**`name` may be a channel index instead of a string**, and for hooks this matters more than it does
for reading. The handlers worth hooking are often generic-named — a group can contain dozens of
channels called `Set Vector` or `Do` — and a lookup by name finds whichever comes first, which is
almost never the one you meant. When you have identified a specific channel by index, pass the
number:

```lua
tw.on_call("Debris.cgr", 48, "after", function() ... end)
```

### `tw.mute(group, name)` → mute handle

Suppresses a channel: the game keeps calling it and it does nothing. Starts active.

Takes an index in place of a name for the same reason as `tw.on_call`.

### `mute:on()` / `mute:off()` / `mute:set(bool)`

Turn suppression on or off without re-hooking. All return the handle, so they chain.

### `mute:active()` → boolean

Whether it is registered *and* currently suppressing.

---

## Drawing

**All of these are ignored outside an `on_frame` handler** — silently, so that drawing from a
channel hook by mistake does not disable the script.

### `tw.hud.text(x, y, text, colour, size, font)`

Draws text with its top-left corner at `(x, y)`. `colour` defaults to opaque white, `size` to the
overlay's own text height, `font` to the default face.

### `tw.hud.measure(text, size, font)` → width, height

What that text would occupy. Measured by the same engine that draws it. **Pass the same `font` you
will draw with** — measuring one face and drawing another is off by enough to be visible in anything
centred or right-aligned, and nothing can catch that for you.

### `tw.hud.font_size()` → number

The overlay's default text height, in pixels. Scale layouts off this.

### `tw.hud.fonts()` → table

Every face name `tw.hud.text` accepts, sorted. Currently `regular` and `semibold`. Faces are weights,
not sizes — `size` and `font` are independent. An unknown name raises an error.

### `tw.hud.rect(x0, y0, x1, y1, colour, rounding, thickness, corners)`

`rounding` is the corner radius (default 0). `thickness` ≤ 0 (the default) fills; positive strokes an
outline of that width. `corners` selects which corners the radius applies to and defaults to all of
them; see [`tw.hud.corners`](#twhudcorners).

### `tw.hud.corners`

A table of corner masks for `tw.hud.rect`: `none`, `top_left`, `top_right`, `bottom_left`,
`bottom_right`, `top`, `bottom`, `left`, `right`, `all`. They are plain bits, so they add:
`tw.hud.corners.top_left + tw.hud.corners.bottom_right`.

This is what a bar built from several abutting rectangles needs — round the outer ends, leave the
internal joins square.

### `tw.hud.line(x0, y0, x1, y1, colour, thickness)`

`thickness` defaults to 1.

### `tw.hud.glow_rect(x0, y0, x1, y1, colour, rounding, strength)`

A soft glow around a rounded rectangle, in the overlay's own style. `strength` is `0..1` and defaults
to 1.

Draws **only** the glow — fill first with `tw.hud.rect`, then glow. That order is also what lets a
shape glow in a different colour than it is filled with.

### `tw.hud.glow_text(x, y, text, colour, glow, size, font, strength)`

Text with a glow behind it. Draws the text as well, unlike the rect version — the glow is offset
copies of the same glyphs, so splitting it in two would rasterize them twice. `glow` defaults to the
text colour.

### `tw.hud.icon(name, x, y, size, colour)`

One of the plugin's built-in SVG icons, in a square box of `size` pixels. `name` is a bare stem —
`"feat_stealth"` — and reaches `icons/feat_stealth.svg` inside the plugin. Icons are monochrome and
take their colour from `colour`.

Scripts cannot load images of their own; see [Limits](limits.md#what-is-deliberately-absent).

---

## Geometry

### `tw.hud.size()` → width, height

The viewport, in pixels.

### `tw.hud.safe()` → x0, y0, x1, y1

The viewport minus the overlay's always-on top band. Full width.

### `tw.hud.widget(name)` → x0, y0, x1, y1 *or* `nil`

Exact rectangle of one overlay widget this frame. `name` is `"notefeed"`, `"pins"`, `"watermark"` or
`"menu"`. `nil` when that widget is not on screen. An unknown name raises an error.

---

## Colours

A colour is a packed integer, `0xAABBGGRR` — alpha, blue, green, red.

### `tw.theme(name)` → colour

A colour from the overlay's live palette. Read at call time, so it follows theme changes. An unknown
name raises an error.

### `tw.theme_names()` → table

Every valid name for `tw.theme`, sorted.

### `tw.rgb(r, g, b, a)` → colour

Packs 0..1 floats. `a` defaults to 1. Designed to take a vector channel's output directly:
`tw.rgb(colour_ch:get())`.

### `tw.alpha(colour, a)` → colour

The same colour with alpha set to exactly `a` (0..1).

### `tw.fade(colour, k)` → colour

The same colour with its existing alpha *scaled* by `k` (0..1). This is the one for fading a whole
widget.

---

## Output and diagnostics

### `tw.notify(message)`

Raises a toast in the overlay's notification strip. For events, not for state — do not call it every
frame.

### `tw.log(message)`

Writes to the plugin log. **Stripped from release builds**, so it is a development tool only.

`print` is an alias for this.

### `tw.warn(message)`

Both of the above: logged *and* shown as a toast. Use this when a script author needs to see
something in a normal install.

### `tw.can_write()` → boolean

Whether writes to the graph are currently accepted. False while the game is still loading, when a
write silently corrupts it rather than failing. A refused write returns `false` like any other failed
write; this exists so a script can wait deliberately.

### `tw.engine_ready()` → boolean

Whether the game's channel graph is reachable at all. False during startup, and until the Tweaker has
captured the engine — which can take until the player interacts with a menu.

### `tw.groups()` → table of strings

Every channel group currently loaded, as `"<pool name> | <file>"`. The answer to "why will my group
not resolve".

---

## Script header annotations

Read from the first comment block **without running the file**. All optional.

```lua
-- @name        Display name (defaults to the file name)
-- @author      Who wrote it
-- @version     Any string
-- @description One line, shown in the Scripts tab
```

Scanning stops at the first line that is neither blank nor a comment.

---

## Standard library

The usual Lua 5.1 / LuaJIT library is present, minus what is listed in
[Limits § What is not in the environment](limits.md#what-is-not-in-the-environment). In particular
`string`, `table`, `math` and `os.clock` / `os.time` / `os.date` all work normally.
