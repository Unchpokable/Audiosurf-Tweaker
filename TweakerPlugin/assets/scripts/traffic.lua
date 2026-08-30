-- @name        Run Tracker
-- @author      Audiosurf Tweaker
-- @version     1.0
-- @description Traffic taken off the road, live bonuses, and a medal bar that counts them
--
-- A run tracker that replaces the game's medal dashboard instead of sitting next to it.
--
-- Three things are going on here, and they are three different demonstrations.
--
--
-- 1. WHAT IT MEASURES - TrafficCommander::Do_CollectCar (#763)
--
-- Not StatCollector::Do_ReportCarCollected, which fires when a block lands *in the grid* - neither
-- necessary nor sufficient for "the player took it off the road". Pointman scooping into the buffer
-- and Eraser shattering both consume traffic and never reach the grid, while abilities dropping
-- blocks back in fire it without consuming anything.
--
-- Do_CollectCar is the single funnel for "the player's car touched this traffic block". It reads the
-- colour once and branches three ways:
--
--     TrafficType := Traffic.Type[CurrentTraffic]        -- the colour, always
--     if SpecialPurpose::DumptyScoopDown?  -> Dumpty_AddOccupant      (Pointman buffer)
--     elseif SpecialPurpose::ShatterStorming? -> Do_LaunchErasers     (Eraser)
--     else                                 -> Puzzle::Do_ResovleLaneCrash  (into the grid)
--
-- All three consume traffic. Being driven by CurrentTraffic - a real traffic index - also means
-- re-injected blocks never come through here, so no discriminator is needed.
--
-- The index on TrafficType is not a typo: TrafficCommander has two Value channels by that name
-- (#631 and #900), and a by-name lookup finds the wrong one. Channel names are not unique.
--
--
-- 2. WHERE THE DENOMINATORS COME FROM - and why not Stats: TrafficColorCounts
--
-- That column is written **once, at the end of the run**, inside Do_CalculateFinalStats, and only
-- for colour ids 0..5. White blocks never enter it at all: Achievements::Do_GetTrafficCounts sorts
-- them into WhiteTotal, which the loop that fills the column does not read. So during a run the
-- column is neither complete nor white-aware, and on a low-traffic track the difference is visible.
--
-- So this walks the same table the game walks - StatCollector::Stats: TrafficPattern, one row per
-- generated block with the colour id in x - and buckets it exactly as ChannelSwitch #1248 does:
--
--     0 purple   1 blue   2 green   3 yellow   4,5 red   7,8 white   6 and 9+ ignored
--
-- The table is complete from the moment the track is generated, so this is correct from the first
-- frame. It is also ~3000 cursor reads, so it is spread over frames rather than done in one.
--
-- Addressed by index (#57) because "Stats: TrafficPattern" is not unique either - there are four.
--
--
-- 3. WHAT IT TAKES OVER - XX_gui::Do_LadderMedalRequirements (#3464)
--
-- The medal dashboard: rings, cover shape, celebration and flash timers all hang off that one
-- ChannelCaller and nothing else does. Nothing downstream reads a value it produces - it only draws
-- - which is why this particular node is safe to mute, and why the choice of node is the entire
-- safety argument.
--
-- Having taken the medals away, this owes the player something better than they were. The game's
-- rings compare raw StatCollector::Points against the thresholds, but the medal actually awarded is
-- decided by PointsWithGridBonus = Points + bonuses. So the bar here counts the bonuses in, which
-- makes it strictly more truthful than the rings it replaced:
--
--     Match7 / Match11 / Match21   +7 / +11 / +21 %, from LargestMatch, mutually exclusive
--     Butter Ninja / Seeing Red    +5 % each, at 95 % of yellow / red, only when not Ninja
--     Clean Finish                 +25 %, only knowable at the end - shown as the second number
--
-- The multipliers are read from the game's own scaler channels rather than hardcoded, so if a patch
-- ever retunes them this follows.
--
-- One honest caveat, and it is the game's own definition, not a shortcut here: the 95 % test uses
-- Achievements::YellowsHit / RedsHit, which is a different quantity from "taken off the road" above.
-- The bonus prediction therefore reads the counters the game will actually test, while the table
-- keeps showing the better metric. Where they disagree is itself interesting.

local COLOURS = {
    -- `vector` is the index of the Value Vector this colour is wired to in XX_StartHere's palette
    -- switch (ChannelSwitch #1535, case n -> colour n), so the swatches show the player's actual
    -- colour settings. `fallback` covers the window before the group resolves.
    --
    -- Colour id 7 is the white/stone block: the palette calls that case "Stone", and
    -- Achievements' traffic switch sorts the same id into WhiteTotal.
    { id = 0, name = "purple", vector = 1542, fallback = 0xFFE08CC8 },
    { id = 1, name = "blue",   vector = 1538, fallback = 0xFFE0C060 },
    { id = 2, name = "green",  vector = 1534, fallback = 0xFF60C060 },
    { id = 3, name = "yellow", vector = 1530, fallback = 0xFF40D0E0 },
    { id = 4, name = "red",    vector = 1526, fallback = 0xFF5050E0 },
    { id = 7, name = "white",  vector = 2667, fallback = 0xFFE0E0E0 },
}

-- Raw colour id -> the row it belongs to, exactly as Achievements' ChannelSwitch #1248 buckets it.
-- Ids missing from this table (6, and 9 upwards) are counted by nobody, in the game or here.
local BUCKET = { [0] = 0, [1] = 1, [2] = 2, [3] = 3, [4] = 4, [5] = 4, [7] = 7, [8] = 7 }

local YELLOW, RED = 3, 4

-- Medal colours are semantic rather than chrome, so they stay literal; everything else comes from
-- the overlay's palette so the widget belongs to Tweaker rather than merely sitting on top of it.
local BRONZE = 0xFF4E7DCD
local SILVER = 0xFFC8C8C8
local GOLD   = 0xFF3FD5F5

-- Run state. Two reads, not a state machine fed by hooks.
--
-- The game already keeps the answer: XX_StartHere::StartupState is its top-level mode, and
-- Stage/XX_WindowState tests it as `StartupState == State_Gameplay` to decide whether the player is
-- actually playing. Comparing the two channels rather than hardcoding 5 costs one extra read and
-- survives the constant being retuned.
--
-- Reading state beats hooking the transitions into and out of it, even though the transitions are
-- easy to find (XX_StartHere::Do_StartSong, XX_PauseScreen::Do_Pause / Do_Unpause). A hook-fed flag
-- is only correct if it observed every edge: load this script mid-run and it starts out wrong, miss
-- one path back to the menu and it stays wrong until the next one. A state read is right on the
-- first frame and cannot desync, for one float per frame.
--
-- Hooks are still the right tool for *events*, which is what Do_ResetStats below is.
local startup_state = tw.float_ch("StartGroup", "StartupState")
local gameplay_state = tw.float_ch("StartGroup", "State_Gameplay")
local paused = tw.float_ch("XX_PauseScreen", "GamePaused?")

local traffic_pattern = tw.array_vec("StatCollector", 57, "Index_TrafficPattern")
local car_count = tw.float_ch("StatCollector", "TotalCarCount")
local points    = tw.float_ch("StatCollector", "Points")
local best_match = tw.float_ch("StatCollector", "LargestMatch")
local tiles_left = tw.float_ch("StatCollector", "NumTilesInGrid")

local bronze_at = tw.float_ch("StartGroup", "BronzeRequirement")
local silver_at = tw.float_ch("StartGroup", "SilverRequirement")
local gold_at   = tw.float_ch("StartGroup", "GoldRequirement")
local league_id = tw.float_ch("StartGroup", "LeagueID")

local scaler = {
    match7  = tw.float_ch("StatCollector", "Match7 Bonus Scaler"),
    match11 = tw.float_ch("StatCollector", "Match11 Bonus Scaler"),
    match21 = tw.float_ch("StatCollector", "Match21 Bonus Scaler"),
    yellow  = tw.float_ch("StatCollector", "YellowNinjaBonusPoints"),
    red     = tw.float_ch("StatCollector", "RedNinja Bonus Scaler"),
    clean   = tw.float_ch("StatCollector", "GridBonusMultiplyer"),
}

local hits = {
    [YELLOW] = tw.float_ch("Achievements", "YellowsHit"),
    [RED]    = tw.float_ch("Achievements", "RedsHit"),
}
local is_ninja = tw.float_ch("SpecialPurpose", "Ninja?")

local type_ch  = tw.float_ch("TrafficCommander", 900)
local scooping = tw.float_ch("SpecialPurpose", "DumptyScoopDown?")
local erasing  = tw.float_ch("SpecialPurpose", "ShatterStorming?")

for _, c in ipairs(COLOURS) do
    c.channel = tw.vector_ch("StartGroup", c.vector)
    c.colour = c.fallback
end

-- The game's own medal rendering, off for as long as this script is loaded. The handle is kept so it
-- can be put back: :off() restores the rings without a reload.
local medals = tw.mute("XX_gui", "Do_LadderMedalRequirements")

local taken = {}
local by_route = { grid = 0, buffer = 0, erased = 0 }

-- 0 hidden, 1 fully shown. Declared up here rather than beside the frame handler so the drawing
-- helpers below can close over it and fade themselves.
local shown = 0

local function reset_tally()
    for _, c in ipairs(COLOURS) do taken[c.id] = 0 end
    by_route.grid, by_route.buffer, by_route.erased = 0, 0, 0
end

reset_tally()

-- The exact "a new run is starting" event, replacing the old heuristic of watching the song timer
-- jump backwards. StatCollector::Do_ResetStats (#1) is what clears the run: it empties
-- Stats: TrafficPattern, zeroes TotalCarCount and resets Points, LargestMatch and the rest, right
-- before the track is regenerated. A timer rewind is a guess about this; this is the thing itself.
tw.on_call("StatCollector", "Do_ResetStats", "after", reset_tally)

-- ---------------------------------------------------------------------------------------------
-- The traffic scan.
--
-- Spread over frames on purpose. Each row costs a cursor write, a vector read and a cursor restore,
-- and a track can be several thousand rows - doing it in one frame would be a visible hitch at the
-- exact moment the player is starting a run.
-- ---------------------------------------------------------------------------------------------
local ROWS_PER_FRAME = 128

-- How long TotalCarCount has to hold still before the scan starts. Track generation grows the table
-- row by row, so the count changes many times on the way up; without this the scan restarts on every
-- one of those frames and does its work several times over for nothing.
local SETTLE_FRAMES = 15

local scan = { at = 0, rows = 0, running = false, ready = false, totals = {}, seen = -1, settled_at = 0 }

local function begin_scan(rows)
    scan.at, scan.rows, scan.running, scan.ready = 0, rows, true, false
    scan.totals = {}
    for _, c in ipairs(COLOURS) do scan.totals[c.id] = 0 end
end

-- Watches TotalCarCount and starts a scan once it has stopped moving. Runs every frame including
-- while the player is still in a menu - the track is generated on the loading screen, so scanning it
-- there is exactly right: by the time the run starts the denominators are already known.
local function watch_track()
    local rows = math.floor((car_count:get() or 0) + 0.5)

    if rows ~= scan.seen then
        scan.seen = rows
        scan.settled_at = tw.frame + SETTLE_FRAMES
        return
    end

    if rows > 0 and rows ~= scan.rows and tw.frame >= scan.settled_at then
        begin_scan(rows)
    end
end

local function step_scan()
    if not scan.running then return end

    local budget = ROWS_PER_FRAME
    while scan.at < scan.rows and budget > 0 do
        local id = traffic_pattern:get(scan.at)
        -- Not resolvable yet (group still loading): stop, keep the position, try next frame.
        if id == nil then return end

        local bucket = BUCKET[math.floor(id + 0.5)]
        if bucket then scan.totals[bucket] = scan.totals[bucket] + 1 end

        scan.at = scan.at + 1
        budget = budget - 1
    end

    if scan.at >= scan.rows then
        scan.running = false
        scan.ready = true
    end
end

-- ---------------------------------------------------------------------------------------------

-- "after": TrafficType is written at the top of the handler, so it is valid by the time this runs,
-- and so are the two ability flags that say which branch was taken.
tw.on_call("TrafficCommander", "Do_CollectCar", "after", function()
    local c = type_ch:get()
    if c == nil then return end

    local bucket = BUCKET[math.floor(c + 0.5)]
    if bucket == nil then return end

    taken[bucket] = (taken[bucket] or 0) + 1

    local scoop = scooping:get()
    local erase = erasing:get()
    if scoop and scoop ~= 0 then
        by_route.buffer = by_route.buffer + 1
    elseif erase and erase ~= 0 then
        by_route.erased = by_route.erased + 1
    else
        by_route.grid = by_route.grid + 1
    end
end)

-- The palette is a player setting, so it is re-read periodically - but not every frame, because five
-- vector evaluations to answer a question that changes when someone opens the options screen is five
-- too many.
local function refresh_palette()
    for _, c in ipairs(COLOURS) do
        local r, g, b = c.channel:get()
        if r then c.colour = tw.rgb(r, g, b) end
    end
end

local function league()
    local id = league_id:get()
    if id == nil then return 1, "casual" end
    -- LeagueID is 0-based: XX_StartHere's Do_CalcMedalRequirements switches the medal scalers on it
    -- directly. The name is printed alongside the rating so a wrong reading is visible rather than
    -- silently folded into the number.
    local n = math.floor(id + 0.5) + 1
    if n < 1 then n = 1 elseif n > 3 then n = 3 end
    return n, ({ "casual", "pro", "elite" })[n]
end

-- Everything the game would add to Points at the end, as far as it is knowable mid-run. Returns the
-- multiplier and the list of feat names, in the game's own order.
local function earned_bonus()
    local mult, feats = 0, {}

    local lm = best_match:get() or 0
    if lm > 20 then
        mult = mult + (scaler.match21:get() or 0.21)
        feats[#feats + 1] = "Match21"
    elseif lm > 10 then
        mult = mult + (scaler.match11:get() or 0.11)
        feats[#feats + 1] = "Match11"
    elseif lm > 6 then
        mult = mult + (scaler.match7:get() or 0.07)
        feats[#feats + 1] = "Match7"
    end

    -- Both colour bonuses are gated on !Ninja? - a Ninja gets Stealth instead, and never these.
    local ninja = is_ninja:get()
    if (ninja or 0) == 0 and scan.ready then
        local yellow_total = scan.totals[YELLOW] or 0
        local yellow_hit = hits[YELLOW]:get()
        if yellow_total > 0 and yellow_hit and yellow_hit / yellow_total >= 0.95 then
            mult = mult + (scaler.yellow:get() or 0.05)
            feats[#feats + 1] = "Butter Ninja"
        end

        local red_total = scan.totals[RED] or 0
        local red_hit = hits[RED]:get()
        if red_total > 0 and red_hit and red_hit / red_total >= 0.95 then
            mult = mult + (scaler.red:get() or 0.05)
            feats[#feats + 1] = "Seeing Red"
        end
    end

    return mult, feats
end

-- Bar spanning 0..gold, split at the bronze and silver thresholds and tinted per zone, filled by
-- score. Overshoot is clamped: an Elite Puzzle run can beat gold by an order of magnitude, and a bar
-- that kept scaling would be a bar that never moves.
local function draw_medal_bar(x0, y0, x1, y1, score, bronze, silver, gold, track, edge)
    tw.hud.rect(x0, y0, x1, y1, track, 3)

    if gold == nil or gold <= 0 then
        tw.hud.rect(x0, y0, x1, y1, edge, 3, 1)
        return
    end

    local w = x1 - x0
    local bx = x0 + w * math.min(bronze or 0, gold) / gold
    local sx = x0 + w * math.min(silver or 0, gold) / gold
    local fill = x0 + w * math.min(math.max(score, 0), gold) / gold

    -- Each zone is drawn only as far as the fill reached, so the bar reads as one advancing bar that
    -- changes colour rather than three independent ones.
    if fill > x0 then tw.hud.rect(x0, y0, math.min(fill, bx), y1, tw.fade(BRONZE, shown), 3) end
    if fill > bx then tw.hud.rect(bx, y0, math.min(fill, sx), y1, tw.fade(SILVER, shown), 0) end
    if fill > sx then tw.hud.rect(sx, y0, fill, y1, tw.fade(GOLD, shown), 0) end

    -- Ticks inset from the top and bottom edges rather than cutting the whole bar: a full-height
    -- divider in the panel colour reads as a gap in the bar.
    local inset = (y1 - y0) * 0.25
    tw.hud.line(bx, y0 + inset, bx, y1 - inset, edge, 1)
    tw.hud.line(sx, y0 + inset, sx, y1 - inset, edge, 1)

    tw.hud.rect(x0, y0, x1, y1, edge, 3, 1)
end

local function centered(x, width, y, text, colour, size)
    local w = tw.hud.measure(text, size)
    tw.hud.text(x + (width - w) * 0.5, y, text, colour, size)
end

-- Is the player actually playing right now? nil from either channel means "could not tell", and the
-- answer to that is yes: hiding a working widget because a lookup failed is a worse failure than
-- showing it somewhere it is not wanted.
local function in_run()
    local state = startup_state:get()
    local gameplay = gameplay_state:get()
    if state == nil or gameplay == nil then return true end
    if math.abs(state - gameplay) > 0.5 then return false end

    -- Only asked once we know we are in gameplay, which is also the only time XX_PauseScreen is
    -- certainly loaded - asking from the main menu would just accumulate "no such group" warnings.
    local held = paused:get()
    return (held or 0) == 0
end

local FADE_PER_FRAME = 0.12

tw.on_frame(function()
    if not tw.engine_ready() then return end

    -- Before the visibility gate, both of them: the track is generated while the loading screen is
    -- up, so the scan has to be allowed to run before the run starts, and a reset arriving while
    -- hidden still has to be honoured.
    watch_track()
    step_scan()

    local target = in_run() and 1 or 0
    if shown < target then
        shown = math.min(target, shown + FADE_PER_FRAME)
    elseif shown > target then
        shown = math.max(target, shown - FADE_PER_FRAME)
    end

    -- Fully faded out: nothing to draw, and nothing below this point should be evaluated either -
    -- the per-frame channel reads are only worth paying for when someone can see the result.
    if shown <= 0 then return end

    if tw.frame % 30 == 0 then refresh_palette() end

    -- Palette: everything structural comes from the overlay's theme, so the widget follows a theme
    -- change instead of drifting out of it. Everything goes through the fade, so entering and
    -- leaving a run dissolves the widget instead of popping it - including the block swatches, which
    -- is why those are faded at the point of use rather than here.
    local function themed(name, weight)
        local colour = tw.theme(name)
        if weight then colour = tw.alpha(colour, weight) end
        return tw.fade(colour, shown)
    end

    local PANEL  = themed("surface", 0.88)
    local EDGE   = themed("border")
    local RULE   = themed("border_subtle")
    local TEXT   = themed("text_primary")
    local DIM    = themed("text_muted")
    local FAINT  = themed("text_faint")
    local ACCENT = themed("accent_text")
    local TRACK  = themed("control_track_off")

    local base = tw.hud.font_size()
    local pad = 10
    local swatch_h = math.max(6, base * 0.55)
    local row_h = base + 7
    local cell_w = math.max(50, base * 4)
    local gap = 16

    local label_size = base * 0.85
    local value_size = base * 2.0
    local bar_h = math.max(12, base)

    local table_w = cell_w * #COLOURS
    local table_h = swatch_h + 5 + row_h * 2
    local right_w = math.max(190, base * 14)
    local right_h = value_size + 8 + bar_h + 4 + label_size

    local inner_h = math.max(table_h, right_h)
    local panel_w = pad * 2 + table_w + gap + right_w
    local panel_h = pad * 2 + inner_h

    -- Bottom-right of the area the overlay is not using. safe() excludes the top band where the
    -- toast column and the watermark live; pins float mid-height on one side and are asked about
    -- separately, since excluding them would make the safe area a hole rather than a rectangle.
    local _, _, sx1, sy1 = tw.hud.safe()
    local x1 = sx1 - 16
    local y1 = sy1 - 16

    local px0, py0, px1, py1 = tw.hud.widget("pins")
    if px0 and px0 < x1 and px1 > x1 - panel_w and py1 > y1 - panel_h and py0 < y1 then
        x1 = px0 - 12
    end

    local x0 = x1 - panel_w
    local y0 = y1 - panel_h

    tw.hud.rect(x0, y0, x1, y1, PANEL, 8)
    tw.hud.rect(x0, y0, x1, y1, EDGE, 8, 1)

    -- ---- the colour table -------------------------------------------------------------------
    local tx = x0 + pad
    local cy = y0 + pad + (inner_h - table_h) * 0.5
    local swatch_y1 = cy + swatch_h
    local totals_y = swatch_y1 + 5
    local taken_y = totals_y + row_h

    tw.hud.line(tx, totals_y - 2, tx + table_w, totals_y - 2, RULE, 1)
    tw.hud.line(tx, taken_y - 2, tx + table_w, taken_y - 2, RULE, 1)

    local got_total, all_total = 0, 0

    for i, c in ipairs(COLOURS) do
        local cx = tx + cell_w * (i - 1)
        local total = scan.ready and scan.totals[c.id] or nil
        local got = taken[c.id] or 0

        local swatch = tw.fade(c.colour, shown)
        tw.hud.rect(cx + 3, cy, cx + cell_w - 3, swatch_y1, swatch, 3)

        if total == nil or total < 1 then
            centered(cx, cell_w, totals_y + 3, "--", FAINT)
            centered(cx, cell_w, taken_y + 3, total == nil and "--" or tostring(got), total == nil and FAINT or DIM)
        else
            got_total = got_total + got
            all_total = all_total + total
            centered(cx, cell_w, totals_y + 3, tostring(total), DIM)
            -- A colour finished off the road reads as that colour rather than as plain text.
            centered(cx, cell_w, taken_y + 3, tostring(got), got >= total and swatch or TEXT)
        end
    end

    -- ---- skill rating and the medal bar -----------------------------------------------------
    local rx = x0 + pad + table_w + gap
    local ry = y0 + pad + (inner_h - right_h) * 0.5

    local score = points:get() or 0
    local gold = gold_at:get()
    local mult, league_name = league()

    local bonus, feats = earned_bonus()
    local clean = scaler.clean:get() or 0.25

    local now_points = score * (1 + bonus)
    local best_points = score * (1 + bonus + clean)

    local function rating(p)
        if gold == nil or gold <= 0 then return 0 end
        return p / gold * 100 * mult
    end

    local label = "SKILL RATING"
    local value = string.format("%.0f", rating(now_points))
    local optimistic = string.format(" (%.0f)", rating(best_points))

    local lw = tw.hud.measure(label, label_size)
    local vw = tw.hud.measure(value, value_size)
    local ow = tw.hud.measure(optimistic, label_size)

    -- One baseline for three different sizes, so the small text sits on the big text's bottom edge.
    -- Exact rather than eyeballed, which is the whole reason a script can measure at all.
    local text_x = rx + (right_w - (lw + 8 + vw + ow)) * 0.5
    local baseline = ry + value_size

    tw.hud.text(text_x, baseline - label_size, label, DIM, label_size)
    tw.hud.text(text_x + lw + 8, baseline - value_size, value, TEXT, value_size)

    -- The optimistic figure is what a Clean Finish would make it. Brightened while the grid actually
    -- is empty, so it stops being a hypothetical and becomes "hold this".
    local grid_empty = (tiles_left:get() or -1) == 0
    tw.hud.text(text_x + lw + 8 + vw, baseline - label_size, optimistic, grid_empty and ACCENT or FAINT, label_size)

    local bar_y = ry + value_size + 8
    draw_medal_bar(rx, bar_y, rx + right_w, bar_y + bar_h, now_points, bronze_at:get(), silver_at:get(), gold, TRACK, EDGE)

    -- Under the bar: what the run has actually banked. Empty is normal early on, so it stays quiet
    -- rather than saying "no feats".
    local feat_line = table.concat(feats, ", ")
    if grid_empty then
        feat_line = feat_line == "" and "Clean Finish" or (feat_line .. ", Clean Finish")
    end
    if feat_line ~= "" then
        centered(rx, right_w, bar_y + bar_h + 4, feat_line, DIM, label_size)
    end

    -- ---- footer -----------------------------------------------------------------------------
    -- The breakdown is the part the game's own counter cannot produce: how much of a run went
    -- through an ability rather than straight into the grid.
    local footer = league_name
    if scan.running then
        footer = footer .. string.format("   scanning track %d%%", 100 * scan.at / math.max(scan.rows, 1))
    elseif all_total > 0 then
        footer = footer .. string.format("   %d/%d traffic  %.0f%%", got_total, all_total, 100 * got_total / all_total)
    end
    footer = footer .. string.format("   grid %d  buffer %d  erased %d", by_route.grid, by_route.buffer, by_route.erased)

    local fh = select(2, tw.hud.measure(footer, label_size))
    tw.hud.text(x0, y0 - fh - 4, footer, FAINT, label_size)
end)
