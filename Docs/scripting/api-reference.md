# API reference

[← back to the index](../scripting.md)

Everything lives in the global table `tw`. It is already there; there is no `require`.

Throughout: `group` is a group name (string), `name` is a channel name (string) **or** a channel
index (number).

---

## Lifecycle

**Nothing you register runs until the game has finished loading.** Your script's chunk is executed
the moment the plugin finds it — which, with the plugin loaded by the game itself, is before the game
has loaded a single channel group — but `on_tick`, `on_post_tick` and `on_frame` are all held back
until the graph is up. So the top level of your script is for *declaring* things (handles, hooks,
constants) and never for reading them.

This is why you no longer need a "has it loaded yet" guard of your own. See
[`tw.state()`](#twstate--string) for what the layer is doing while you wait, and
[`tw.on_ready(fn)`](#twon_readyfn) for the one callback that fires when it stops waiting.

There are two frames, and which one your handler runs on matters.

| | Runs on | For |
|---|---|---|
| `tw.on_tick` / `tw.on_post_tick` | the **engine** frame — one per evaluation of the game's channel graph | reading channels, computing, keeping state |
| `tw.on_frame` | the **overlay** frame — one per drawn frame of the overlay | drawing, and nothing else |

They are not the same rate. The overlay's frame does not happen while the window is minimised, does
not happen before the game has a device, and runs as fast as the machine allows. The engine's frame
is when the values you are reading actually change.

**Rule of thumb: compute in `on_tick`, draw in `on_frame`.**

**Register at the top level of your script, or inside `tw.on_ready` — nowhere else.** Every `tw.on_*`
call and `tw.mute` made from inside some other callback is refused and reported once in the Scripts
tab. Handlers run in the order they were registered, and registering from a callback that runs every
frame is a list that grows every frame. Passing something that is not a function is an error on the
spot, at your line.

**Each script fails on its own.** Every handler runs under its own error guard: when yours throws,
you lose that one call, and every other script — and every other handler of yours — runs as if
nothing happened. A script that keeps failing, or keeps taking too long, is *suspended* rather than
left to drag everything down. See [Limits § Failure containment](limits.md#failure-containment).

### `tw.on_ready(fn)`

Registers `fn` to run **once**, on the first frame where everything is allowed: the graph is up,
channels resolve, writes land.

It also fires for a script enabled in the middle of a session, on that script's next frame — the
contract is "once, when it can", not "once, at startup". That is the whole point: a script switched
on mid-run needs the same setup a script loaded at boot needs, and neither has to detect which case
it is in.

```lua
local points = tw.float_ch("StatCollector", "Points")   -- declare at the top level

tw.on_ready(function()
    -- everything works from here
    tw.notify("score tracker armed at " .. tostring(points:get()))
end)
```

### `tw.on_state(fn)`

Registers `fn(state)` to run on every change of [`tw.state()`](#twstate--string), with the new state
as a string. Unlike everything else here, **this runs in all states**, including while the game is
still loading — it is how the layer reports what it is doing.

### `tw.on_group(name, fn)`

Registers `fn(loaded)` to run when that channel group appears or disappears. `name` is a pool name
(`"Renderer"`) or a bare file name (`"Puzzle"`), the same two spellings every other call accepts.

This is the event a script that lives inside a run actually wants. The game destroys and rebuilds
whole groups as you play — the renderer pool goes on *every* run — and before this there was no way
to know.

```lua
tw.on_group("Renderer", function(loaded)
    if loaded then reset_my_caches() end
end)
```

Like `on_state`, it runs in all states.

### `tw.on_tick(fn)`

Registers `fn` to run once per engine frame, immediately **before** the game evaluates its channel
graph. Call at the top level; several handlers per script are allowed and run in registration order.

Drawing from here does nothing — there is no frame open around it, and the `tw.hud.*` calls quietly
refuse rather than corrupting anything.

### `tw.on_post_tick(fn)`

The same, immediately **after** the graph has been evaluated. This is where the results of the frame
that just happened are readable.

### `tw.on_frame(fn)`
### `tw.on_frame(fn, { before_ready = true })`

Registers `fn` to run once per drawn overlay frame, for as long as the script is enabled. Call at the
top level. Several handlers per script are allowed; they run in registration order.

This is the only place drawing works.

Like everything else, it does not run until the game has loaded. For the rare script that really
does want to draw over the loading screen, pass `{ before_ready = true }`: that handler, and only that
one, runs from the first frame. Channels may not resolve yet and writes are refused while the game
loads, so draw with what you have.

### `tw.on_unload(fn)`

Registers `fn` to run when the script is switched off or reloaded — before its hooks are taken out,
so it can still reach the channels it held. The place to put back whatever the script changed.

Every `on_unload` handler runs, even if an earlier one throws. It does **not** run when the game
itself exits: there is nothing left to put back by then. Writes follow the usual rule and are refused
while the game is loading.

### `tw.frame`

The **engine** frame number: one per evaluation of the game's channel graph. Read-only in practice,
and monotonic. Useful for throttling:

```lua
if tw.frame % 30 == 0 then refresh_something_expensive() end
```

> **Changed.** This used to count overlay frames. A script written against the old meaning still
> works, but `% 60` now means "about once a second" on every machine instead of "once a second on a
> 60 Hz one" — which is what it was always meant to mean.

### `tw.draw_frame_count`

Overlay frames drawn since the VM started, for the rare thing that really is about drawing rate.
Almost always the wrong number to throttle on; `tw.frame` is the right one.

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
### `tw.matrix_ch(group, name)` → handle

Create a handle for a numeric, text, vector or matrix channel. Cheap; never fails; does not touch the game.
Resolution happens on first use and is retried until it succeeds.

`tw.channel` is a deprecated alias for `tw.float_ch`.

### `handle:get()`

- float → number, or `nil`
- text → string, or `nil`
- vector → three numbers, or `nil`
- matrix → sixteen numbers, row-major (`_11 _12 _13 _14 _21 … _44`), or `nil`

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

### `handle:set(m11, m12, …, m44)` → boolean *(matrix handles)*
### `handle:set(table)` → boolean *(matrix handles)*

Writes all sixteen elements, either as sixteen arguments or as one table of sixteen numbers in the
same row-major order. Returns `false` if it could not be resolved. Subject to the same rules as any
other write — see [Limits](limits.md#writing-to-the-game).

### `handle:live()` → boolean *or* `nil`

Whether the game evaluated this channel **in the current frame**: `true`, `false`, or `nil` when that
cannot be known — some channels are never cached by the engine, and for those nothing records when
they were last computed.

"The current frame" is literal. From `on_post_tick`, right after the game evaluated its graph, it
tells you whether a part of the game is running right now as opposed to merely loaded. From
`on_tick`, which runs *before* the graph, it is `false` for everything, because nothing has been
evaluated yet this frame.

It does not tell you whether the value is "real" — a channel you have just read yourself counts as
evaluated, because reading it is what evaluates it.

### `handle:valid()` → boolean

Whether this handle has resolved yet. Does not attempt a resolve.

### `handle:dead_end()` → boolean

Whether this handle has **given up**, which is a different thing from not having resolved yet.

A handle retires when the group it names is loaded and has no channel by that name — a group's
channel list is fixed once it loads, so no amount of waiting will produce one. That is a typo, it is
reported once, and the handle stops looking. A handle that is merely waiting for its group (the
groups of a run, asked for from the menu) is not a dead end and never becomes one.

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
any other return proceeds — and so does a handler that throws, and one whose script is suspended. A
script can never take a piece of the game away by failing. Returning `false` from `"after"` does
nothing.

**A group that is not loaded is not a problem.** The subscription is accepted immediately and
attached to the real channel whenever that group turns up — and attached again, by itself, if the
group is destroyed and rebuilt, which is what happens to the renderer pool on every run. You hook a
channel once, at the top level, and never think about it again.

A name that will never resolve is the other case, and it is loud: if the group is loaded and has no
such channel, you are told once and the subscription is dropped.

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

Takes an index in place of a name for the same reason as `tw.on_call`, and waits for its group the
same way.

A mute belongs to its script: while the script is suspended, the game gets the channel back. A HUD
script that mutes the game's own HUD and then breaks leaves the player with the game's HUD, not with
nothing.

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

### `tw.hud.gradient_rect(x0, y0, x1, y1, from, to, vertical)`

A rectangle filled with a two-stop linear gradient: `from` at the left edge and `to` at the right,
or top and bottom when `vertical` is true. Both colours carry their own alpha, so a backdrop that
fades out to nothing is the same call as one colour fading into another.

No rounding — the primitive underneath is one quad with per-corner colours and has no rounded form.
A gradient that wants a soft end gets it from the gradient.

Three of these make the usual shape: a plain `rect` for the part that has to be solid, and one
gradient either side of it fading to `tw.alpha(colour, 0)`.

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
frame. Capped per script: a handful a minute, then one line saying so, and the rest are collected in
the script's messages in the Scripts tab.

### `tw.log(message)`

Writes to the plugin log. **Stripped from release builds**, so it is a development tool only.

`print` is an alias for this.

### `tw.warn(message)`
### `tw.error(message)`
### `tw.pending(message)`

Messages for the script's author, filed in the script's row in the Scripts tab — which, unlike the
log, exists in a normal install. All three remember where they were said from (`myhud.lua:88`), and
saying the same thing from the same place again only increases a counter next to it: calling one of
these every frame is one line, not thousands.

| | Scripts tab | Notification strip |
|---|---|---|
| `tw.warn` | yes, with a count | never |
| `tw.error` | yes, with a count | once per message, and only once the game has loaded |
| `tw.pending` | shows the script as **waiting** | never |

`tw.error` reports; it does not raise. Use Lua's own `error()` to stop.

`tw.pending` is a statement about *now*: the script shows as waiting for as long as it keeps saying
it, and stops showing as waiting a couple of seconds after it stops. Say it every frame you are
waiting — "waiting for a run to start" — and the row reads correctly by itself.

> **Changed.** `tw.warn` used to show a toast as well. It no longer does: the notification strip is
> for things the *player* should see, and those are `tw.notify` and `tw.error`.

### `tw.state()` → string

Where the game is, as one of:

| | Meaning |
|---|---|
| `"detached"` | the plugin has not seen the game run a frame yet |
| `"booting"` | the game is starting — its own loader is still in charge |
| `"starting"` | the game has handed over, and is still assembling itself |
| `"ready"` | everything works |
| `"busy"` | was ready, and the game is loading content again — starting a run, for instance |

`"busy"` is not a fault. It happens on every single run, and your callbacks keep running through it.

### `tw.ready()` → boolean

True in `"ready"` and `"busy"`: the graph is up and reading, writing and hooking all work.

You rarely need to call it. Your handlers do not run in any other state, so inside `on_tick`,
`on_post_tick` and `on_frame` it is always true. It is there for `on_state` and `on_group`, which do
run in every state.

### `tw.can_write()` → boolean

An alias for `tw.ready()`, kept for scripts written before it. Writes used to have a gate of their
own; they do not any more, because "has the game finished loading" turned out to be one question with
one answer.

### `tw.engine_ready()` → boolean

Whether the game's channel graph is **reachable** — a weaker thing than `tw.ready()`, and a different
question. It goes true about a second into the process, as soon as the game has run its first frame,
which is long before the game has finished loading.

Use `tw.ready()` to decide whether to act. This is here for diagnostics.

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
