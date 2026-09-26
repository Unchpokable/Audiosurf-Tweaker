-- The top strip: the projected score, personal best and the queue of points in flight
-- (DESIGN.md §8, §9, §10).

local colours = require("colours")
local ch = require("channels")
local feed = require("feed")
local roll = require("roll")
local grouped = require("draw.text").grouped

local COLOUR_BY_ID, BUCKET, WHITE, AHEAD, BEHIND = colours.COLOUR_BY_ID, colours.BUCKET, colours.WHITE, colours.AHEAD, colours.BEHIND
local personal_best, points_l, points_r, use_clone = ch.personal_best, ch.points_l, ch.points_r, ch.use_clone
local FLOATER_FADE_IN = feed.FADE_IN

return function(L)
    local fade, TEXT, DIM, FAINT, WARN = L.fade, L.TEXT, L.DIM, L.FAINT, L.WARN
    local base, screen_w = L.base, L.screen_w
    local grid_empty, now_points, alt_points = L.grid_empty, L.now_points, L.alt_points
    -- Taken once per frame: the queue is only ever replaced by a reset hook, never mid-frame.
    local floaters = feed.floaters()

    -- ---- score, personal best and the points in flight, top-centre ---------------------------
    --
    -- Centred on the viewport, like the chain block at the bottom - the two are read as one column
    -- down the middle of the screen, and anchoring one of them to something else would break that.
    -- The score gains a digit a handful of times per run and grows symmetrically about the centre
    -- when it does, which is half a glyph of movement; the line below it changes width on every
    -- match, so that one's left edge is rolled rather than recomputed.
    do
        local score_cx = screen_w * 0.5
        local score_size = base * 2.2
        local best_size = base * 0.95
        local float_size = base * 1.1

        -- Measured from the top of the VIEWPORT, not of the safe area. The safe area exists to keep
        -- a widget clear of the overlay's own top chrome, and honouring it here put the score a
        -- tenth of the way down the screen - far enough in that it sat over the track and blocked
        -- the read on oncoming traffic. The chrome it is avoiding lives in the top corners; this is
        -- centred and narrow, so it goes past.
        local pad_x = math.max(18, base * 1.6)
        local pad_y = math.max(6, base * 0.5)
        local back_y0 = math.max(4, base * 0.3)
        local top = back_y0 + pad_y

        -- The headline is the PROJECTED final score, not StatCollector::Points - the same figure the
        -- skill rating is built from, and the only one the personal best can be compared against
        -- (DESIGN.md §10). It still moves when Points moves, because it is a fixed multiple of it, so
        -- the "the number lands and the total takes it" contract survives intact.
        local shown_score = roll("score", now_points, 0.45)
        local score_text = grouped(shown_score)
        local sw, sh = tw.hud.measure(score_text, score_size, "semibold")
        local score_x = score_cx - sw * 0.5

        -- The other side of Clean Finish, in brackets, exactly as the skill rating carries it: above
        -- the headline while the bonus is still to be had, below it once it is banked and at risk.
        local alt_text = string.format("(%s)", grouped(roll("score_alt", alt_points, 0.45)))
        local alt_gap = 8
        local alt_w = tw.hud.measure(alt_text, best_size)

        -- Everything is measured before anything is drawn: the backdrop has to be sized from the
        -- content it sits behind, and it has to go down first. The rows are not all centred on the
        -- same point - the score is, and the bracket hangs off its right - so the extents are
        -- tracked rather than a single width.
        local content_l = score_x
        local content_r = score_x + sw + alt_gap + alt_w
        local rows_h = sh

        local function span(x0, x1)
            content_l = math.min(content_l, x0)
            content_r = math.max(content_r, x1)
        end

        local coop_text, coop_w, coop_h = nil, 0, 0
        if (use_clone:get() or 0) ~= 0 then
            coop_text = string.format("L %s      R %s", grouped(roll("score_l", points_l:get() or 0, 0.45)),
                grouped(roll("score_r", points_r:get() or 0, 0.45)))
            coop_w, coop_h = tw.hud.measure(coop_text, best_size, "semibold")
            span(score_cx - coop_w * 0.5, score_cx + coop_w * 0.5)
            rows_h = rows_h + coop_h + 2
        end

        -- The game gates its own popup on PB > 0, and so does this: a song never played has no
        -- record, and "best 0" is a worse answer than no answer.
        local best = personal_best:get()
        local best_text, delta_text, best_w, delta_w, best_h = nil, nil, 0, 0, 0
        local delta = 0
        local best_gap = 10
        if best ~= nil and best > 0 then
            best_text = "best " .. grouped(best)
            delta = shown_score - best
            -- grouped() carries the minus itself, so only the positive case needs a sign added.
            delta_text = (delta >= 0 and "+" or "") .. grouped(delta)
            best_w = tw.hud.measure(best_text, best_size)
            delta_w, best_h = tw.hud.measure(delta_text, best_size, "semibold")
            span(score_cx - (best_w + best_gap + delta_w) * 0.5, score_cx + (best_w + best_gap + delta_w) * 0.5)
            rows_h = rows_h + best_h + 2
        end

        -- ---- the backdrop ------------------------------------------------------------------
        --
        -- Without one the score sits on whatever the track is doing behind it, and on a bright
        -- skybox white text on white is simply gone. The game puts a solid bar there for exactly
        -- this reason; this is the same idea without the bar.
        --
        -- Flat top and bottom, and a horizontal fade out to nothing on both sides. The first stop is
        -- the end of the content, so the part that is actually behind text is at full strength and
        -- everything past it is the falloff. Symmetric on purpose - the queue of points extends to
        -- the left only, but a backdrop that grew on one side to follow it would be a backdrop that
        -- changes shape on every match.
        local half = math.max(score_cx - content_l, content_r - score_cx) + pad_x
        local falloff = math.max(base * 6, screen_w * 0.13)
        local back_y1 = back_y0 + rows_h + pad_y * 2
        local back = tw.fade(tw.alpha(tw.theme("surface"), 0.72), fade)
        local clear = tw.alpha(back, 0)

        tw.hud.gradient_rect(score_cx - half - falloff, back_y0, score_cx - half, back_y1, clear, back)
        tw.hud.rect(score_cx - half, back_y0, score_cx + half, back_y1, back)
        tw.hud.gradient_rect(score_cx + half, back_y0, score_cx + half + falloff, back_y1, back, clear)

        -- ---- the rows ----------------------------------------------------------------------
        tw.hud.text(score_x, top, score_text, TEXT, score_size, "semibold")
        -- On the score's own baseline rather than its top, so the small figure sits on the big one's
        -- bottom edge. Which of the two it is is never in doubt - it is above the headline in one
        -- case and below it in the other - so it is tinted by direction rather than labelled, the
        -- same way the skill rating carries it.
        tw.hud.text(score_x + sw + alt_gap, top + sh - best_size, alt_text, grid_empty and WARN or FAINT, best_size)

        local next_y = top + sh + 2

        -- Double Vision keeps two scores and the game shows them separately; so do we, on their own
        -- row under the total rather than in place of it. The points in flight are NOT routed to a
        -- side: which half a match belongs to is only decided at Do_UpdatePoints time, by
        -- IncomingPointsAreRight? / IncomingPointsAreSharedMatch?, and by then our number has been
        -- on screen for the whole of the particle's flight.
        if coop_text ~= nil then
            tw.hud.text(roll("coop_x", score_cx - coop_w * 0.5, 0.25), next_y, coop_text, DIM, best_size, "semibold")
            next_y = next_y + coop_h + 2
        end

        if best_text ~= nil then
            local bx = roll("best_x", score_cx - (best_w + best_gap + delta_w) * 0.5, 0.25)
            tw.hud.text(bx, next_y, best_text, FAINT, best_size)
            -- Tinted rather than labelled, and only ever two states: still behind, or already past.
            tw.hud.text(bx + best_w + best_gap, next_y, delta_text, tw.fade(delta >= 0 and AHEAD or BEHIND, fade), best_size,
                "semibold")
        end

        -- ---- the queue ---------------------------------------------------------------------
        --
        -- Laid out right to left from the score's leading edge, each entry behind the one in front
        -- of it by its own width plus a gap. The slots are recomputed every frame from whoever is
        -- still here, so when the number at the head leaves - because the last of its particles has
        -- landed and the total has just taken its points - the rest slide up into the space.
        --
        -- That sliding is the whole reason the position is not read from the particle any more. It
        -- used to be, and with one number in the air it looked right; with a multicolour collection
        -- it put every colour of the match at the same place at the same time, which is what the
        -- queue exists to stop.
        local queue_gap = math.max(12, base * 1.3)
        local cursor = score_x - queue_gap
        local float_y = top + (sh - float_size) * 0.5

        for _, f in ipairs(floaters) do
            local visible = f.age - f.delay
            if visible > 0 then
                -- What this match is worth to the headline, not what it was worth raw. The factor is
                -- common to every point in the run - the game's scalers are all over raw Points and
                -- never compounded - so distributing it over the numbers is exact rather than an
                -- approximation: they still sum to the total (DESIGN.md §10).
                local worth = f.amount * f.mult
                local text = (worth >= 0 and "+" or "") .. grouped(worth)
                local fw = tw.hud.measure(text, float_size, "semibold")
                local slot = cursor - fw
                cursor = slot - queue_gap

                -- Eased towards the slot rather than placed in it, so the queue closes up smoothly.
                -- A new entry starts where it belongs; only existing ones move.
                if f.x == nil then
                    f.x = slot
                else
                    f.x = f.x + (slot - f.x) * math.min(1, tw.dt() / 0.12)
                end

                -- An unrecognised id is drawn neutral rather than guessed at: the ram bonus files
                -- its points under colour 11 and the special-purpose path under 14, neither of which
                -- is a block colour at all (DESIGN.md §8).
                local entry = f.colour_id and COLOUR_BY_ID[BUCKET[f.colour_id] or -1] or nil
                local colour = (worth < 0) and BEHIND or (entry and entry.colour or WHITE)

                local weight = math.min(1, visible / FLOATER_FADE_IN) * (1 - f.gone)
                -- On the way out it covers the last of the distance into the score, so the number
                -- reads as going in rather than as being switched off next to it.
                local x = f.x + (score_x - f.x) * f.gone * 0.4
                local c = tw.fade(tw.alpha(colour, weight), fade)

                tw.hud.glow_text(x, float_y, text, c, c, float_size, "semibold", 0.55 * weight * fade)
            end
        end
    end
end
