-- PuzzlePRO HUD - entry point.
--
-- Why this HUD is built the way it is - what it measures, where every number comes from, which of the
-- game's widgets it takes over and why each of those cuts is safe - is docs/DESIGN.md, and the
-- modules refer to it by section (DESIGN.md §N). Read it before changing anything that reads the game.
--
-- The order of the requires below is the order things happen in, and it is the order the script did
-- them in when it was one file: every channel handle is created, then the game's own widgets are
-- muted, then the hooks are registered. The last part is not cosmetic - the tally and the score feed
-- both hook Do_ResetStats and Do_ResetSimpleStats, and two handlers on one call run in the order
-- they were added.

local ch = require("channels")
local palette = require("palette")
-- Required for what they do on load: the mutes go on, and the tally and the score feed hook the game.
require("mutes")
local scan = require("scan")
require("tally")
local track = require("track")
local feed = require("feed")
local scoring = require("scoring")

local draw_stats_panel = require("draw.stats_panel")
local draw_rating = require("draw.rating")
local draw_score_strip = require("draw.score_strip")
local chain_block = require("draw.chain_block")

local colours = require("colours")
local BRONZE, SILVER, GOLD = colours.BRONZE, colours.SILVER, colours.GOLD

local startup_state, gameplay_state, paused = ch.startup_state, ch.gameplay_state, ch.paused
local points, gold_at, tiles_left, scaler = ch.points, ch.gold_at, ch.tiles_left, ch.scaler
local step_scan, begin_scan, totals = scan.step, scan.begin, scan.totals
local step_floaters = feed.step
local refresh_palette = palette.refresh
local league, earned_bonus = scoring.league, scoring.earned_bonus

-- 0 hidden, 1 fully shown.
local shown = 0

-- What every section of the frame draws with: sizes, theme colours, the safe area, and what the
-- sections before it found out. One table, refilled every frame rather than rebuilt.
local L = {}

-- Is the player actually playing right now? nil from either channel means "could not tell", and the
-- answer to that is yes: hiding a working widget because a lookup failed is a worse failure than
-- showing it somewhere it is not wanted.
local function in_run()
    local state = startup_state:get()
    local gameplay = gameplay_state:get()
    if state == nil or gameplay == nil then
        return true
    end
    if math.abs(state - gameplay) > 0.5 then
        return false
    end

    -- Only asked once we know we are in gameplay, which is also the only time XX_PauseScreen is
    -- certainly loaded - asking from the main menu would just accumulate "no such group" warnings.
    local held = paused:get()
    return (held or 0) == 0
end

-- Time-based rather than per-frame: the game does not run at a fixed frame rate, and a constant
-- step per frame fades at whatever speed the machine happens to reach.
local FADE_SECONDS = 0.18

tw.on_frame(function()
    if not tw.engine_ready() then
        return
    end
    -- Before the visibility gate: the track is generated and dressed while the loading screen is
    -- still up, so the walk has to be allowed to run before the run starts.
    step_scan()
    track.step()
    -- Same reason, different failure: a number in flight when the player hits pause must keep
    -- ageing, or it comes back on screen and replays a match that finished a minute ago.
    step_floaters()

    -- The one case the hooks cannot cover: this script was enabled part-way through a run, so the
    -- events that would have armed the scan are long past.
    if not totals.ready and not totals.running and tw.frame % 60 == 0 then
        begin_scan()
    end
    if not track.ready and not track.running and tw.frame % 60 == 0 then
        track.begin()
    end

    local target = in_run() and 1 or 0
    local step = tw.dt() / FADE_SECONDS
    if shown < target then
        shown = math.min(target, shown + step)
    elseif shown > target then
        shown = math.max(target, shown - step)
    end

    -- Fully faded out: nothing to draw, and nothing below this point should be evaluated either -
    -- the per-frame channel reads are only worth paying for when someone can see the result.
    if shown <= 0 then
        return
    end

    if tw.frame % 30 == 0 then
        refresh_palette()
    end

    -- Eased so the widget arrives and leaves with some weight instead of ramping linearly.
    local fade = tw.ease("cubicOut", shown)

    -- Everything structural comes from the overlay's theme, so the widget follows a theme change
    -- instead of drifting out of it. Everything goes through the fade, so entering and leaving a run
    -- dissolves the widget instead of popping it - including the block swatches, which is why those
    -- are faded at the point of use rather than here.
    local function themed(name, weight)
        local colour = tw.theme(name)
        if weight then
            colour = tw.alpha(colour, weight)
        end
        return tw.fade(colour, fade)
    end

    local RULE = themed("border_subtle")
    local TEXT = themed("text_primary")
    local DIM = themed("text_muted")
    local FAINT = themed("text_faint")
    local WARN = themed("text_warning")
    local TRACK = themed("control_track_off")

    -- Tier -> rail colour, index 1 being "no medal yet". That one is the overlay's own neutral
    -- rather than a fourth metal, so the rail starts grey and warms into bronze. Raw rather than
    -- themed(): the rail mixes between two of these and fades the result once, and fading twice
    -- would make the transition dip in the middle.
    local MEDAL_STOPS = { tw.theme("text_muted"), BRONZE, SILVER, GOLD }

    local base = tw.hud.font_size()
    local screen_w = tw.hud.size()

    local value_size = base * 1.15
    local label_size = base * 0.85
    local rating_size = base * 2.0
    local chain_size = base * 1.6

    local swatch = math.max(10, base * 0.8)
    local row_gap = 5
    local row_h = value_size + 6
    local cell_w = math.max(swatch + 14, base * 2.8)

    -- The medal rail is the chain rails' own thickness, on purpose: it was the one filled solid on a
    -- screen of hairlines, and matching the thickness is most of what stops it reading as a blot.
    local rail_h = math.max(3, base * 0.18)
    local medal_dot = math.max(4, rail_h * 1.6)
    local medal_gap = math.max(3, base * 0.22)
    local medal_h = rail_h + medal_gap + medal_dot

    -- The top of the safe area is deliberately not taken: the two widgets that live up there are
    -- measured from the viewport instead (DESIGN.md §9), and everything else hangs off the bottom.
    local sx0, _, sx1, sy1 = tw.hud.safe()
    local margin = 16
    local bottom = sy1 - margin

    L.fade = fade
    L.RULE, L.TEXT, L.DIM, L.FAINT, L.WARN, L.TRACK = RULE, TEXT, DIM, FAINT, WARN, TRACK
    L.MEDAL_STOPS = MEDAL_STOPS
    L.base, L.screen_w = base, screen_w
    L.value_size, L.label_size, L.rating_size, L.chain_size = value_size, label_size, rating_size, chain_size
    L.swatch, L.row_gap, L.row_h, L.cell_w = swatch, row_gap, row_h, cell_w
    L.rail_h, L.medal_dot, L.medal_gap, L.medal_h = rail_h, medal_dot, medal_gap, medal_h
    L.sx0, L.sx1, L.sy1, L.margin, L.bottom = sx0, sx1, sy1, margin, bottom

    -- The bottom band: one strip that the song badge, the chain block and the rating all sit in, so
    -- the three read as a row. As tall as the chain block at its fullest (draw/chain_block.lua says why).
    L.band_y1 = bottom
    L.band_y0 = bottom - chain_block.band_height(L)

    -- What is to the left of the chain block in the band - its right edge. Nothing yet: the colour
    -- table that used to be there is in the stats panel now.
    L.left_r = sx0 + margin

    -- ---- the colour table and the minimap, top-left (draw/stats_panel.lua) -----------------
    draw_stats_panel(L)

    -- ---- what the run is currently worth ----------------------------------------------------
    --
    -- One projection, three consumers: the skill rating, the score at the top, and the multiplier
    -- the points in flight are scaled by. They used to be two different ideas of "the score" on the
    -- same screen, and the top one was the raw figure - which made its comparison against the
    -- personal best meaningless, because a personal best is PointsWithGridBonus (DESIGN.md §10).
    local score = points:get() or 0
    local gold = gold_at:get()
    local mult, league_name = league()

    local bonus, feats = earned_bonus()
    local clean = scaler.clean:get() or 0.25
    local grid_empty = (tiles_left:get() or -1) == 0

    -- What the game would score if the run ended on this frame (DESIGN.md §7).
    --
    -- The Clean Finish bonus is not a hypothetical the player might reach: the game adds it whenever
    -- the board is empty **at the moment it scores**, so while the board is clear it is already
    -- theirs. Leaving it out of the headline under-reported by a fifth exactly while a good player
    -- was holding a clean board - which is most of a good run, and the whole of the end of one.
    local live_bonus = bonus + (grid_empty and clean or 0)
    -- The other side of that: what it becomes if the board fills, or what it would become if it
    -- emptied. Whichever it is, it is the figure the player is not currently on.
    local alt_bonus = grid_empty and bonus or (bonus + clean)

    local now_points = score * (1 + live_bonus)
    local alt_points = score * (1 + alt_bonus)

    -- Published for the score feed, which runs from channel hooks and cannot reach these channels
    -- cheaply (a launch happens once per collected TILE). DESIGN.md §10.
    feed.set_multiplier(1 + live_bonus)

    L.gold, L.mult, L.feats, L.grid_empty = gold, mult, feats, grid_empty
    L.now_points, L.alt_points = now_points, alt_points

    -- ---- skill rating and the medal bar, bottom-right (draw/rating.lua) ----------------------
    draw_rating(L)

    -- ---- score, personal best and the points in flight, top-centre (draw/score_strip.lua) ----
    draw_score_strip(L)

    -- ---- the chain, its drop timer and the feats, bottom-centre (draw/chain_block.lua) -------
    chain_block.draw(L)

    -- The footer below was switched off before the script was split into modules; kept as it was.
    -- Its names now live in L (league_name aside, which is `select(2, league())`) and in tally.
    --
    -- -- ---- footer, above the table ------------------------------------------------------------
    -- -- The breakdown is the part the game's own counter cannot produce: how much of a run went
    -- -- through an ability rather than straight into the grid, and how many blocks the game invented.
    -- local footer = league_name
    -- if false then
    --     -- (nothing to report while loading now that the totals come from the game)
    -- elseif all_total > 0 then
    --     footer = footer .. string.format("   %d/%d traffic  %.0f%%", got_total, all_total, 100 * got_total / all_total)
    -- end
    -- footer = footer .. string.format("   grid %d  buffer %d  erased %d", by_route.grid, by_route.buffer, by_route.erased)
    -- if powerups > 0 or ghosts > 0 then
    --     footer = footer .. string.format("   powerups %d  ghosts %d", powerups, ghosts)
    -- end

    -- local fh = select(2, tw.hud.measure(footer, label_size))
    -- tw.hud.text(tx, swatch_y - fh - 6, footer, FAINT, label_size)
end)
