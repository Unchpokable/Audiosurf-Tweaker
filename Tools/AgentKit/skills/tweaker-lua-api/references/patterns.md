# Patterns

Working shapes for the common jobs. Every channel named here is one a script shipped with the Tweaker already
uses (`puzzlepro_hud`, the project templates); anything else you take from the docs or from `cgrpy` is a lead
until the game has shown it to you.

## 1. Read a value and draw it

```lua
-- Top level: a handle, created once. It finds its channel the first time it is read.
local points = tw.float_ch("StatCollector", "Points")

tw.on_frame(function()
    local score = points:get()
    if score == nil then
        return -- not in a run: the group does not exist in the menu
    end

    -- Never a fixed position, and not tw.hud.safe() (pattern 4): ask for a free spot near the corner.
    local text = string.format("Score: %.0f", score)
    local w, h = tw.hud.measure(text)
    local x, y = tw.hud.place(w, h, "top_left", 20)
    if x == nil then
        return -- no room this frame
    end
    tw.hud.text(x, y, text, tw.theme("text_primary"))
end)
```

## 2. Count something the game does

A hook gets no arguments and runs on the game's own call stack: it only counts. Drawing happens in `on_frame`.

```lua
local collected = 0

local function one_more()
    collected = collected + 1
end

-- Two handlers: a power-up block goes to Do_CollectQuestionBox and never reaches Do_CollectCar.
tw.on_call("TrafficCommander", "Do_CollectCar", "after", one_more)
tw.on_call("TrafficCommander", "Do_CollectQuestionBox", "after", one_more)

-- A new run and a restart both reset the run's statistics; reset ours with them. A restart from the pause menu
-- fires Do_ResetSimpleStats and NOT Do_ResetStats - hooking only the latter keeps counting into the old run.
local function run_started()
    collected = 0
end
tw.on_call("StatCollector", "Do_ResetStats", "after", run_started)
tw.on_call("StatCollector", "Do_ResetSimpleStats", "after", run_started)

tw.on_frame(function()
    local x, y = tw.hud.place(160, 24, "bottom_left")
    if x == nil then
        return -- no free room this frame
    end
    tw.hud.text(x, y, "Blocks: " .. collected, tw.theme("text_primary"))
end)
```

## 3. What the event was about

Read what the handler itself wrote, from an `"after"` hook — never the handler's group parameters, which hold
whatever they held last.

Blocks per colour are the case where the obvious script is wrong. Count **track rows**, each once, from both
collection handlers: a power-up block never reaches `Do_CollectCar`, and the block the game re-births in its slot
(a "ghost") is a copy of a row that will still spawn. Counting events instead is off per colour in both directions
and passes 100 % on a colour cleared to the last block (`gameplay-map.md`, "Collecting a block").

```lua
-- #900, not the name: TrafficCommander has two channels called TrafficType, and the first (#631) is not the one
-- the collection handlers write. Found with `cgrpy query find "^TrafficType$" --group TrafficCommander`.
local traffic_type = tw.float_ch("TrafficCommander", 900)
local traffic_id = tw.float_ch("TrafficCommander", "TrafficID") -- the track row, written by Do_CollectCar
-- Do_CollectQuestionBox does not write TrafficID; its row is this column at the power-up's slot.
local row_of_slot = tw.array("TrafficCommander", "Traffic: PrecalcedArrayNum", "CurrentTraffic")
local current_traffic = tw.float_ch("TrafficCommander", "CurrentTraffic")

local by_colour, seen = {}, {}

local function count_row(row, colour)
    if colour == nil then
        return
    end
    if row ~= nil and row >= 0 then
        if seen[row] then
            return -- a ghost: this row was already counted
        end
        seen[row] = true
    end
    local c = math.floor(colour + 0.5)
    by_colour[c] = (by_colour[c] or 0) + 1
end

tw.on_call("TrafficCommander", "Do_CollectCar", "after", function()
    count_row(traffic_id:get(), traffic_type:get())
end)

tw.on_call("TrafficCommander", "Do_CollectQuestionBox", "after", function()
    local slot = current_traffic:get()
    count_row(slot and row_of_slot:get(slot), traffic_type:get())
end)

local function run_started()
    by_colour, seen = {}, {}
end
tw.on_call("StatCollector", "Do_ResetStats", "after", run_started)
tw.on_call("StatCollector", "Do_ResetSimpleStats", "after", run_started)
```

The other side of a percentage — how many blocks of each colour the track has — is a walk of
`StatCollector::Stats: TrafficPattern`, the way the game's own `Do_GetTrafficCounts` buckets it, re-done after
`Do_ReseteWhiteWildBlocks` (whites and wilds are placed over coloured rows on every run). **Not**
`Stats: TrafficColorCounts`: it is counted before whites exist and is wrong for the whole run.

Colour numbers and what each means to the player: `.tweaker/docs/internals/gameplay-map.md`, "Colour numbers".

## 4. Replace a piece of the game's HUD

Mute the game's drawing, draw your own. The mute is the script's: if the script is suspended or switched off,
the game's own score comes back by itself — the player is never left with nothing.

```lua
-- XX_gui #2351 is the top strip in one cut: its frame, the score and the game's "+N" floaters. By index because
-- the handler's name says nothing; puzzlepro_hud mutes the same node.
local game_score = tw.mute("XX_gui", 2351)

local points = tw.float_ch("StatCollector", "Points")

-- The first box already on screen that overlaps this one, or nil.
local function overlap(x0, y0, x1, y1)
    for _, r in ipairs(tw.hud.occupied()) do
        if x0 < r.x1 and r.x0 < x1 and y0 < r.y1 and r.y0 < y1 then
            return r
        end
    end
end

tw.on_frame(function()
    local score = points:get()
    if score == nil then
        return
    end
    local text = string.format("%.0f", score)
    local w, h = tw.hud.measure(text, 40, "semibold")
    local sw = tw.hud.size()
    -- The score's own place, where the game's was: top centre, from the top of the viewport. Not tw.hud.safe() - it
    -- removes a full-width band as tall as the notification feed, and a score measured from it sits over the track.
    -- The overlay's widgets live in the corners, so this is normally free; checked, not assumed - something above
    -- in the run order may be there, and then we sit below it.
    local x0, y0 = (sw - w) / 2 - 16, 4
    local x1, y1 = x0 + w + 32, y0 + h + 8
    for _ = 1, 4 do
        local r = overlap(x0, y0, x1, y1)
        if r == nil then
            break
        end
        y0, y1 = r.y1 + 8, r.y1 + 8 + (y1 - y0)
    end
    -- Drawn every frame, so drawing it is what takes the box; tw.hud.claim is for frames you skip (pattern 5).
    -- Something dark behind it, or it vanishes on a white sky.
    tw.hud.rect(x0, y0, x1, y1, tw.rgb(0, 0, 0, 0.45), 6)
    tw.hud.text(x0 + 16, y0 + 4, text, tw.theme("text_primary"), 40, "semibold")
end)
```

`tw.hud.safe()` is in the API from before scripts shared the screen. It answers "below everything the overlay could
ever put at the top, across the whole width" — far too coarse to lay out by. **Lay out with `place`, `occupied` and
`claim`**; leave `safe()` alone.

Before muting anything else: `cgrpy query parents XX_gui <index>` and `cgrpy query dec XX_gui <index>`. A node that
only draws can go; a node something else reads cannot (`.tweaker/docs/internals/gameplay-map.md`, the HUD section,
lists which is which). The one that looks like a render branch and is not: `XX_gui::Do_RenderMoneyParticles`
(#3980) is the loop that **pays the score** of every match — mute it and the player scores nothing for the rest of
the run.

**What the player compares the score with.** `StatCollector::Points` is the raw score; the personal best (`XX_gui::PB`)
and the medals at the end are the **final** score, with bonuses — 20–50 % more over a good run. A HUD that puts
`Points` next to the record tells a good player they are having a bad run (`gameplay-map.md`, "The end of the run").

## 5. Share the screen

```lua
local W, H = 220, 64

tw.on_frame(function()
    -- The free spot nearest the corner, clear of the overlay's widgets and of the scripts that ran before this
    -- one; taken as soon as it is returned.
    local x, y = tw.hud.place(W, H, "top_right")
    if x == nil then
        return
    end
    tw.hud.rect(x, y, x + W, y + H, tw.alpha(tw.theme("surface"), 0.8), 8)
    tw.hud.text(x + 12, y + 12, "Hello", tw.theme("text_primary"))
end)
```

A widget that blinks or fades keeps its box on the frames it is not drawn: `tw.hud.claim(x0, y0, x1, y1)` every
frame, or the scripts after it move into its place and back. Theme names: `tw.theme_names()`; an unknown one is
an error — check the list in the game or in `.tweaker/docs/scripting/drawing.md`.

## 6. Survive the game rebuilding a group

The renderer pool is destroyed and rebuilt on every run. Handles and hooks follow by themselves; your own cache of
what you derived from that group does not.

```lua
tw.on_group("Renderer", function(loaded)
    if loaded then
        reset_my_caches()
    end
end)
```

## 7. Throttle

```lua
local league_id = tw.float_ch("StartGroup", "LeagueID") -- 0 Casual, 1 Pro, 2 Elite
local league

tw.on_tick(function()
    if tw.frame % 30 ~= 0 then
        return -- a few times a second is plenty for a value that changes once a run
    end
    league = league_id:get()
end)
```

`StartGroup` is the game's root group, `XX_StartHere.cgr`; both names work.

## 8. Write, and put it back

A write is the last resort: trace the channel first (who feeds it, who reads it — skill `cgrpy`), keep the original
and restore it.

```lua
local target = tw.float_ch("SomeGroup", "SomeValue") -- traced: what feeds it, what reads it
local original

tw.on_ready(function()
    original = target:get()
end)

tw.on_tick(function()
    target:set(42)
end)

tw.on_unload(function()
    if original ~= nil then
        target:set(original)
    end
end)
```

A value the game writes every frame wins against yours on the next frame: writing it from `on_tick` (before the
graph) is overwritten by the graph; from `on_post_tick` it lasts until the next evaluation. Which one you need
depends on who reads it — another reason to trace first.

## 9. Probe: is this the handler I think it is?

The fastest way to learn what a channel does is to watch it.

```lua
-- Probe: delete before shipping.
tw.on_call("TrafficCommander", "Do_CollectCar", "after", function()
    tw.notify("Do_CollectCar fired")
end)
```

`tw.notify` is capped at a handful a minute; for something that fires every frame, count it and draw the count
(pattern 2), or record it (pattern 11). Then ask the user what they saw — skill `audiosurf-in-game-loop`.

## 10. Settings the player changes in the game, kept between sessions

A tab of the script's own in the overlay menu (`.tweaker/docs/scripting/menus.md`) and a settings file in its own
folder (`.tweaker/docs/scripting/storage.md`). API 3: `"api": 3` in `twproject.json`.

```lua
-- Top level: the settings file is read once, here - the first read is a real disk read, so not in a frame.
-- No file yet (first run) is nil, and `or {}` turns that into "everything at its default".
local cfg = tw.storage.read_json("settings.json") or {}

-- Labels in English: the overlay's fonts have no Cyrillic.
local tab = tw.ui.tab{ label = "My HUD" }
local look = tab:group("Look")
local show = look:toggle{ label = "Show the panel", value = cfg.show ~= false } -- missing means on
local scale = look:slider{ label = "Scale", min = 0.5, max = 2, value = cfg.scale or 1 }
local corner = look:choice{ label = "Corner", options = { "Top left", "Top right" }, value = cfg.corner or "Top right" }
local reset = tab:button{ label = "Reset to defaults" }

local function save()
    -- A choice by its text: the file survives an option inserted in the middle of the list later.
    cfg.show, cfg.scale, cfg.corner = show:get(), scale:get(), corner:get().text
    tw.storage.write_json("settings.json", cfg) -- returns at once; the disk gets it within 0.1 s
end

for _, control in ipairs({ show, scale, corner }) do
    control:on_change(function(_, committed)
        if committed then
            save() -- a slider is committed when the player lets go, not on every step of the drag
        end
    end)
end

reset:on_click(function()
    -- set() does not call on_change, so save here.
    show:set(true)
    scale:set(1)
    corner:set("Top right")
    save()
end)

tw.on_frame(function()
    if not show:get() then
        return
    end
    local s = scale:get()
    local w, h = 200 * s, 48 * s
    local x, y = tw.hud.place(w, h, corner:get().index == 1 and "top_left" or "top_right")
    if x == nil then
        return
    end
    tw.hud.rect(x, y, x + w, y + h, tw.alpha(tw.theme("surface"), 0.85), 8)
    tw.hud.text(x + 12 * s, y + 12 * s, "My HUD", tw.theme("text_primary"), 20 * s)
end)
```

Save on `committed`, never only at exit: `on_unload` does not run when the game quits. The worked version that
was checked in the game is `.tweaker/docs/scripting/examples.md`, "A settings tab that survives a restart".

## 11. Probe: record values over a run into a file you can read

The log keeps the first occurrence of a message, not a value over time, and the user cannot read numbers off a
moving HUD. A probe can write them into the script's own folder —
`<game>\engine\TweakerStuff\ScriptData\<id>\` — and you read the file after the user's run. A **Mark** button in
the probe's tab lets the user say "now" at the moment they care about.

```lua
-- Probe: delete before shipping (its tab and its file go with it; the file itself stays in ScriptData).
local points = tw.float_ch("StatCollector", "Points")
local chain = tw.float_ch("Puzzle", "ChainCount")

local lines, bytes = {}, 0
local LIMIT = 900 * 1024 -- under the 1 MiB file limit

local function add(line)
    if bytes + #line + 1 <= LIMIT then
        lines[#lines + 1] = line
        bytes = bytes + #line + 1
    end
end

local tab = tw.ui.tab{ label = "Probe" }
local recording = tab:toggle{ label = "Record", value = true }
local marks = 0
tab:button{ label = "Mark this moment" }:on_click(function()
    marks = marks + 1
    add(string.format("%d mark %d", tw.frame, marks))
end)

local function run_started()
    lines, bytes = {}, 0
    add(string.format("%d run started", tw.frame))
end
tw.on_call("StatCollector", "Do_ResetSimpleStats", "after", run_started)

tw.on_tick(function()
    if recording:get() and tw.frame % 10 == 0 then
        local p, c = points:get(), chain:get()
        if p ~= nil and c ~= nil then
            add(string.format("%d points %.0f chain %d", tw.frame, p, c))
        end
    end
    -- The whole file is rewritten each time: a few times a second, not every frame.
    if tw.frame % 120 == 0 and #lines > 0 then
        tw.storage.write_text("probe.txt", table.concat(lines, "\n") .. "\n")
    end
end)
```

Tell the user what to do ("play one song as Mono; press **Mark this moment** in the overlay's *Probe* tab when the
chain drops") and that the file is written while they play. Read it, never write it, and take the probe out
afterwards — skill `audiosurf-in-game-loop`.
