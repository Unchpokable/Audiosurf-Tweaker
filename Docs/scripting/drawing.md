# Drawing

[← back to the index](../scripting.md)

Everything a script draws happens inside `tw.on_frame`, in immediate mode: there are no objects to
create and nothing persists. What you draw this frame is what is on screen this frame.

```lua
tw.on_frame(function()
    tw.hud.text(100, 100, "hello")
end)
```

**Drawing only works from `on_frame`.** A `tw.hud.*` call from a channel hook silently does nothing —
those run inside the game's own logic, where there is no frame open to draw into. It is ignored
rather than raising an error, because an error there would fire from inside the game's graph walk and
disable a script whose only mistake was drawing from the wrong callback.

The pattern is: accumulate in the hook, draw in `on_frame`.

## Coordinates

Pixels, origin top-left, `y` growing downward.

**Never hardcode positions.** The player picked the resolution, not you.

```lua
local w, h = tw.hud.size()                  -- the whole viewport
local x0, y0, x1, y1 = tw.hud.safe()        -- the part free of overlay chrome
```

`tw.hud.safe()` is the viewport with the overlay's own top band (the toast column and the watermark)
removed. It is full width, so a bottom-right corner is `x1, y1`:

```lua
local _, _, x1, y1 = tw.hud.safe()
tw.hud.text(x1 - 200, y1 - 40, "bottom right-ish")
```

### Exact widget geometry

For lining up against a specific piece of the overlay:

```lua
local x0, y0, x1, y1 = tw.hud.widget("notefeed")
```

Valid names: `"notefeed"`, `"pins"`, `"watermark"`, `"menu"`. Returns `nil` when that widget is not
on screen — no pins are showing, the menu is closed. An unknown name is an error, so a typo is loud.

These are **this frame's** rectangles. Scripts draw after the overlay has laid itself out, so you can
sit flush against the toast column, or beside an open menu and follow it while it is dragged.

Note the direction of the relationship: scripts draw **last**, so script output goes *on top of*
overlay chrome. The geometry is for lining yourself up, not for the overlay's benefit. Staying tidy
is your script's job.

## Text

```lua
tw.hud.text(x, y, text, colour, size, font)
```

`colour`, `size` and `font` are optional — omitted, you get opaque white at the overlay's own text
size in its default face. `(x, y)` is the top-left corner of the text.

```lua
local w, h = tw.hud.measure(text, size, font)   -- what it will occupy
local base = tw.hud.font_size()                 -- the overlay's default text height
local faces = tw.hud.fonts()                    -- { "regular", "semibold" }
```

Faces are **weights, not sizes**: `size` and `font` are independent axes, and any face draws crisply
at any size. Use `semibold` for numbers a player reads at a glance and `regular` for labels — that is
what the built-in tracker does.

> **Measure with the face you draw with.** `tw.hud.measure(text, size)` and
> `tw.hud.measure(text, size, "semibold")` give different answers, and using one to position the
> other is a few pixels of drift that looks like nothing in a screenshot and like a misaligned
> column on screen.

`tw.hud.measure` measures with the same engine that draws, so alignment is exact rather than
approximate. This is what makes centring and right-alignment possible at all:

```lua
local function centred(x, width, y, text, colour, size)
    local w = tw.hud.measure(text, size)
    tw.hud.text(x + (width - w) * 0.5, y, text, colour, size)
end
```

Scale your layout off `tw.hud.font_size()` rather than assuming pixel sizes — it follows the
overlay's font, which is not the same on every machine.

## Shapes

```lua
tw.hud.rect(x0, y0, x1, y1, colour, rounding, thickness)
tw.hud.line(x0, y0, x1, y1, colour, thickness)
```

For `rect`, `thickness` of `0` or less (the default) fills it; anything positive strokes an outline of
that width. `rounding` is the corner radius.

A panel with a border is the two together:

```lua
tw.hud.rect(x0, y0, x1, y1, panel_colour, 8)        -- fill
tw.hud.rect(x0, y0, x1, y1, border_colour, 8, 1)    -- outline
```

### Choosing which corners round

`tw.hud.rect` takes an eighth argument selecting which corners the radius applies to:

```lua
local C = tw.hud.corners
tw.hud.rect(x0, y0, mid, y1, colour, 4, 0, C.left)      -- rounded left end, square right
tw.hud.rect(mid, y0, x1, y1, other, 4, 0, C.right)
```

Masks are plain bits and add: `C.top_left + C.bottom_right`. `C.none` really means none.

This exists for bars built out of several abutting rectangles, and it matters more than it sounds.
Round all four corners of each segment and the joins between them show a notch of whatever is
behind; leave them all square and the last segment's corner escapes from under the rounded outline
drawn over it. Both look like rendering glitches and neither is.

### Glow

```lua
tw.hud.glow_rect(x0, y0, x1, y1, colour, rounding, strength)
tw.hud.glow_text(x, y, text, colour, glow, size, font, strength)
```

The overlay's own glow, so a script's highlights look like the overlay's rather than like a script's.
`strength` is `0..1`.

`glow_rect` draws only the glow — fill first, then glow, which is also how you get a shape that
glows in a different colour than it is filled with. `glow_text` draws the text too, because the glow
is offset copies of the same glyphs and doing it in two calls would rasterize them twice.

```lua
tw.hud.rect(x0, y0, x1, y1, colour, 3)
tw.hud.glow_rect(x0, y0, x1, y1, colour, 3, 0.6)
```

### Icons

```lua
tw.hud.icon("feat_stealth", x, y, size, colour)
```

One of the icons packed into the plugin, in a square box, tinted. `name` is a bare stem; it reaches
`icons/<name>.svg` inside the plugin and nothing else. They are monochrome by design — the colour is
yours.

A script cannot supply images of its own. There is no circle and no polygon either. If you need one,
say so — the surface is small because it grew from what scripts actually asked for.

## Colours

A colour is a packed integer, `0xAABBGGRR`. Note the byte order: **alpha, blue, green, red**. Writing
them by hand is error-prone, so mostly you should not.

### From the overlay's theme

This is the one that matters for anything that should look like it belongs:

```lua
tw.theme("surface")
tw.theme("text_primary")
tw.theme("border")
```

`tw.theme` reads the overlay's **live** palette. If the user changes the theme, or edits a colour in
the Settings tab, your widget follows — because the value is read at call time, not cached.

A script that invents its own greys looks foreign in the overlay and drifts further out of place with
every theme change. Backgrounds, borders, muted text: take them from the theme. Keep literals for
colours that carry *meaning* — a medal is bronze regardless of the theme.

`tw.theme_names()` returns every valid name, sorted. An unknown name is an error rather than a silent
black. The useful ones:

| | |
|---|---|
| `surface`, `surface_muted`, `surface_elevated` | panel backgrounds |
| `border`, `border_subtle`, `border_strong` | outlines and rules |
| `text_primary`, `text_secondary`, `text_muted`, `text_faint` | text, in descending prominence |
| `accent_primary`, `accent_text`, `accent_border` | highlights |
| `text_error`, `text_warning` | problems |
| `control_track_off` | the empty part of a slider or bar |

### From the game

Vector channels hold the player's own block colours:

```lua
local purple = tw.vector_ch("StartGroup", 1542)

local r, g, b = purple:get()
local packed = r and tw.rgb(r, g, b) or 0xFFE08CC8
```

`tw.rgb(r, g, b, a)` takes 0..1 floats — exactly what a vector channel gives you — so
`tw.rgb(colour_ch:get())` is the whole path from the game's live palette to a drawing call. Alpha
defaults to opaque.

### Adjusting

```lua
tw.alpha(colour, 0.5)     -- set alpha to exactly 0.5
tw.fade(colour, 0.3)      -- scale the alpha it already has by 0.3
```

`tw.fade` is the one you want for fading a whole widget in and out: every colour keeps its relative
weight, which does not happen if you force them all to the same absolute alpha.

```lua
local shown = 0
local FADE_SECONDS = 0.18

tw.on_frame(function()
    local target = should_be_visible() and 1 or 0
    local step = tw.dt() / FADE_SECONDS
    shown = shown + math.max(-step, math.min(step, target - shown))
    if shown <= 0 then return end        -- nothing to draw, and nothing below costs anything

    tw.hud.rect(x0, y0, x1, y1, tw.fade(tw.theme("surface"), tw.ease("cubicOut", shown)), 8)
end)
```

Note `tw.dt()` rather than a fixed step per frame. The game's frame rate is not fixed and can be very
high, so a constant per-frame step is a fade whose duration depends on the machine. `tw.ease` then
shapes the result — see [the API reference](api-reference.md#tweasecurve-t--number).

## A worked layout

Putting it together — a panel in the bottom-right, sized to its content, themed, aligned:

```lua
tw.on_frame(function()
    local base = tw.hud.font_size()
    local pad = 10

    local title = "Run"
    local value = string.format("%.0f", points:get() or 0)

    local tw_, th = tw.hud.measure(title)
    local vw, vh = tw.hud.measure(value, base * 2)

    local panel_w = math.max(tw_, vw) + pad * 2
    local panel_h = th + vh + pad * 2 + 4

    local _, _, sx1, sy1 = tw.hud.safe()
    local x1, y1 = sx1 - 16, sy1 - 16
    local x0, y0 = x1 - panel_w, y1 - panel_h

    tw.hud.rect(x0, y0, x1, y1, tw.alpha(tw.theme("surface"), 0.88), 8)
    tw.hud.rect(x0, y0, x1, y1, tw.theme("border"), 8, 1)

    tw.hud.text(x0 + pad, y0 + pad, title, tw.theme("text_muted"))
    tw.hud.text(x0 + pad, y0 + pad + th + 4, value, tw.theme("text_primary"), base * 2)
end)
```

Nothing is hardcoded except the padding: it sizes to its text, sits in the corner at any resolution,
and matches whatever theme the user is running.

## Notifications

For things that are events rather than state, use the overlay's toast strip instead of drawing:

```lua
tw.notify("New personal best")
```

Do not call this every frame. It is a notification, not a display.

## Next

[API reference](api-reference.md), or [Limits, safety and performance](limits.md).
