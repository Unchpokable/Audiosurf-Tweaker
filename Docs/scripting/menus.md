# Menu tabs

[← back to the index](../scripting.md)

A script can put **its own tab into the overlay menu** and fill it with controls — switches, sliders,
numbers, choices, colours, buttons, lists, text — and then read what the player set, hear about it the
moment it changes, and change the controls itself. It is how a script gets settings.

```lua
local tab = tw.ui.tab{ label = "My HUD" }

local look   = tab:group("Look")
local show   = look:toggle{ label = "Show the panel", value = true }
local scale  = look:slider{ label = "Scale", min = 0.5, max = 2, value = 1 }
local corner = look:choice{ label = "Corner", options = { "Left", "Right" }, value = "Right" }

tw.on_frame(function()
    if not show:get() then return end
    draw_panel(scale:get(), corner:get().text)    -- your drawing
end)
```

**You declare the tab; the plugin draws it.** A tab is not drawn from your script: you describe it once,
when the script loads, and the overlay draws it every frame it is open with the same controls as its own
tabs. Your script reads the values with `:get()` whenever it likes. That has three consequences worth
knowing up front:

- a tab costs your script **nothing** while the menu is closed, and nothing while it is open either — no
  Lua runs to draw it;
- a script that throws, is suspended or is reloaded cannot leave a tab half-drawn;
- the values are there before the game has loaded and on every frame, so `on_tick` can read them too.

The price is layout: a tab is a column of rows and groups, not a canvas. For settings that is what you
want.

Settings that survive a restart are your script's job, and one call each way — see
[Saving settings](#saving-settings) below and [Saving data](storage.md).

---

## Where the tab goes

After the overlay's own tabs (Skins, Tweaks, Player, Settings, Skybox, Scripts). Scripts' tabs follow
the [order scripts run in](getting-started.md#the-order-scripts-run-in), and a script's own tabs the
order it declared them. Dragging a script in the Scripts tab moves its tabs with it. **Reload keeps the
tab where it was** — and selected, if it was: a reloaded script that declares the same tabs gets the
same places.

**One tab is usually right.** Sections go into [groups](#groups) on the tab, and the page scrolls by
itself. There is no limit on tabs, but every tab is a button in the menu's column for every player who
installs your script.

**Labels that clash get your script's name.** When a tab's label is the same as another tab's —
a built-in one or another script's, ignoring case — every script tab with that label shows as
`<script name> - <label>`: a `Settings` tab of yours reads `My HUD - Settings`, because the menu has a
Settings tab of its own. The name is your header's `@name`, or the file name without `.lua`. A tab with
no label at all is named after the script.

**When your script stops, the tab follows it.** Suspended (it failed or ran too long — see
[Limits](limits.md#failure-containment)): the tab stays, its label dims, and the page is switched off
as a whole; the player can still open it and see the values, and **Resume** in the Scripts tab brings it
back. Switched off, or failed to load: the tab is gone, and the menu selects the one before it.

## Declaring

```lua
local tab = tw.ui.tab{ label = "My HUD" }     -- or tw.ui.tab("My HUD")
local volume = tab:slider{ label = "Volume", min = 0, max = 1, value = 0.8, tooltip = "How loud" }
```

**Only while the script loads, or from `tw.on_ready`** — the same window as every other registration
([API reference § Lifecycle](api-reference.md#lifecycle)). A tab or a control declared from any other
callback is refused and reported once in the Scripts tab; what you get back is an *inert* handle, which
answers `:get()` with the value you gave and does nothing else, so the next line of your script does not
fail over the first mistake.

**Every constructor takes a table.** A bare string is short for `{ label = "..." }`. Every control
accepts:

| Field | |
|---|---|
| `label` | The text beside or above it. |
| `tooltip` | Shown when the pointer rests on its row. |
| `enabled` | `false` declares it switched off: it shows its value and takes no input. |
| `visible` | `false` declares it hidden: it takes no room at all. |

and the fields of its kind, below. A field of the wrong type — `min = "zero"` — is an error at your line.

### The controls

| Constructor | Value — what `:get()` returns | Its own fields |
|---|---|---|
| `toggle{...}` | `true` / `false` | `value` (default `false`) |
| `slider{...}` | a number in `min..max` | `min` (0), `max` (1), `value` (default `min`) |
| `number{...}` | a whole number in `min..max` | `min` (0), `max` (100), `value` (default `min`) |
| `choice{...}` | `{ text = ..., index = ... }` — the selected option | `options` (required, a list of strings), `value` (an option's text or number; default the first) |
| `color{...}` | a colour, packed like [`tw.rgb`](drawing.md#colours) | `value` (default white) |
| `list{...}` | the selected row's number, from 1, or `nil` | `items` (strings, or `{ text = ..., sub = ... }` for a row with a second line), `value`, `height` (pixels, 160) |
| `button{...}` | — a click is an event, see [`on_click`](#hearing-about-changes) | — |
| `text(...)` | — a paragraph, wrapped to the page | `text`; `tab:text("...")` is short for `tab:text{ text = "..." }` |
| `separator()` | — a line across the page | — |

A value outside the range is clamped, a number is rounded, a choice's `value` that is not one of its
options is left at the first option with a warning in the Scripts tab — a settings file written by an
older version of your script can name an option the new one dropped.

**A choice answers with its option, not a string.** `corner:get()` is `{ text = "Right", index = 2 }`,
so you compare whichever is clearer — `corner:get().text == "Right"` or `corner:get().index == 2`. There
is one such table per option, made when the options are, and `:get()` returns **the same table** every
time: calling it every frame makes no garbage. Do not change those tables.

### Groups

```lua
local look = tab:group("Look")
local size = look:slider{ label = "Size" }
local more = look:group("More")             -- a group in a group: as deep as it goes
```

A group is a framed section across the whole width of the page, with its title in the frame. It takes
every constructor a tab takes, groups included — **two levels deep at most**; a third is refused, with
what you tried to put in it. A hidden group hides what is in it, a switched-off group switches off what
is in it.

## Reading and writing

**`:get()` works anywhere, any time** — in `on_frame`, in `on_tick`, on the loading screen, in your
script's body. It is one call into the plugin, cheap enough for every frame.

**`:set(value)` is your script writing.** It takes the same kinds of value `:get()` returns (a choice
takes the option's text or its number; a list, a row number or `nil`), does nothing when the value is
the same, and **never raises `on_change`** — a script that reacts to its own writes would loop.

**The player wins.** While the player is dragging a slider or a colour, your `:set()` is ignored and
the drag carries on; when they let go, their value is what you hear about. A script that writes a
control every frame does not make it jump under the player's hand.

## Hearing about changes

```lua
scale:on_change(function(value, committed)
    preview(value)
    if committed then save() end
end)

reset:on_click(function()
    scale:set(1)          -- set() does not call on_change; do what you need here
    save()
end)
```

`control:on_change(fn)` runs `fn(value, committed)` whenever **the player** changes the control —
`value` as `:get()` would return it. `committed` is `true` when the change is final: always for a
toggle, a number, a choice and a list, and for a slider or a colour once the player lets go. While a
slider is being dragged you hear the values on the way with `committed = false` — enough to preview
something live, and the place to save is where `committed` is `true`.

`button:on_click(fn)` runs `fn()` on every click.

Both are registrations: in the script's body or `tw.on_ready`, like any `tw.on_*`; a handler that is
not a function is an error at your line. You can register several on one control; they run in order.

**When they run:** in the same overlay frame as the click, before any `on_frame` — so your drawing
already shows the reaction — and only once the game has loaded, like every other callback. A control
the player moves several times before your handler gets to run (on the loading screen, say) is
reported **once**, with its last value, `committed` if any of the changes was. A handler that throws
counts against your script exactly like an `on_frame` that throws.

## Changing a control later

Everything you declared can change afterwards, from anywhere — including from `on_frame`:

```lua
status:set(string.format("%d runs this session", runs))   -- a text paragraph
mode:options({ "Off", "Simple", "Detailed" })             -- a choice's options; the selection is clamped
history:items(recent)                                     -- a list's rows
scale:label("Scale (" .. preset .. ")")
scale:tooltip(nil)                                        -- no tooltip
advanced:show(expert:get())                               -- hide or show
apply:enable(dirty)                                       -- switch on or off
```

These cost a comparison when nothing changed, so writing the same text every frame is fine. Building
the text is another matter: `tab:visible()` says whether your tab was on screen this frame or the last,
so a status line that is expensive to put together can wait until somebody is looking.

```lua
tw.on_frame(function()
    if tab:visible() then status:set(describe_everything()) end
end)
```

Every setter returns the handle, so calls chain: `look:toggle{ label = "Extra" }:on_change(fn)`.

## Saving settings

The tab does not remember anything by itself — values start as you declared them every time the script
loads. Saving is your script's, with [`tw.storage`](storage.md):

```lua
local cfg = tw.storage.read_json("settings.json") or {}

local tab    = tw.ui.tab{ label = "My HUD" }
local show   = tab:toggle{ label = "Show the panel", value = cfg.show ~= false }
local scale  = tab:slider{ label = "Scale", min = 0.5, max = 2, value = cfg.scale or 1 }
local corner = tab:choice{ label = "Corner", options = { "Left", "Right" }, value = cfg.corner or "Right" }

local function save()
    cfg.show, cfg.scale, cfg.corner = show:get(), scale:get(), corner:get().text
    tw.storage.write_json("settings.json", cfg)
end

for _, control in ipairs({ show, scale, corner }) do
    control:on_change(function(_, committed)
        if committed then save() end
    end)
end
```

Read once at the top; write on `committed`. `cfg.show ~= false` makes "missing" mean "on", which is
what a first run with no file wants. Save a choice by its text rather than its number: the file then
survives you inserting an option in the middle of the list.

There is no "reset to defaults" built in — a button and a few `:set()` calls are, and what counts as a
default is yours to decide.

## Limits

| | |
|---|---|
| A label, a tooltip, an option, a list row | 96 bytes; longer is cut, and the Scripts tab says so |
| A text paragraph | 1024 bytes |
| Options in a choice | 16 |
| Rows in a list | 512 |
| Controls per script, its tabs included | 1024; past that, declarations are refused |
| Groups | two levels |

**Latin text only, for now.** The overlay's fonts carry the Latin range; letters outside it — Cyrillic,
Greek, CJK — draw as `?` in labels as everywhere else in the overlay.

**Not there yet:** typing text into a field, catching a key, drawing your own picture inside the tab (a
preview of your HUD), tabs inside a tab, icons in list rows, a step for sliders. If one of those is
what stands between you and your script, say so.

---

## Next

[Saving data](storage.md) — where `tw.storage` keeps your files and what JSON turns into. Every
function on one page: [API reference § Menu tabs](api-reference.md#menu-tabs).
