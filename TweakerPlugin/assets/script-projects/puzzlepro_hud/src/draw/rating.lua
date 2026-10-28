-- The skill rating and the medal rail, bottom-right (DESIGN.md §7).

local ch = require("channels")
local roll = require("roll")
local draw_medal_rail = require("draw.medal_rail")

local bronze_at, silver_at = ch.bronze_at, ch.silver_at

return function(L)
    local fade, TEXT, DIM, FAINT, WARN, TRACK, MEDAL_STOPS = L.fade, L.TEXT, L.DIM, L.FAINT, L.WARN, L.TRACK, L.MEDAL_STOPS
    local base, label_size, rating_size = L.base, L.label_size, L.rating_size
    local rail_h, medal_dot, medal_gap, medal_h = L.rail_h, L.medal_dot, L.medal_gap, L.medal_h
    local sx1, margin, band_y0, band_y1 = L.sx1, L.margin, L.band_y0, L.band_y1
    local gold, mult, grid_empty, now_points, alt_points = L.gold, L.mult, L.grid_empty, L.now_points, L.alt_points

    local right_w = math.max(190, base * 14)
    local right_h = rating_size + 8 + medal_h
    local rx1 = sx1 - margin
    -- Centred in the bottom band the chain block and the song badge share (main.lua), so the three read
    -- as one row rather than as three blocks each sitting on the bottom edge at its own height.
    local ry = (band_y0 + band_y1 - right_h) * 0.5

    -- Pins float mid-height on one side and are the one piece of overlay chrome the safe area
    -- cannot express, so they are asked about separately.
    local px0, py0, px1, py1 = tw.hud.widget("pins")
    if px0 and px0 < rx1 and px1 > rx1 - right_w and py1 > ry and py0 < ry + right_h then
        rx1 = px0 - 12
    end

    local rx = rx1 - right_w

    local function rating(p)
        if gold == nil or gold <= 0 then
            return 0
        end
        return p / gold * 100 * mult
    end

    -- Score arrives in bursts - a big match is worth thousands at once - so the rating is rolled
    -- rather than printed live. The two figures are rolled separately: they move by different
    -- amounts, and a single tween applied to both would drag the second one around by the first.
    local label = "SKILL RATING"
    local value = string.format("%.0f", roll("rating", rating(now_points)))
    local alt = string.format(" (%.0f)", roll("rating_alt", rating(alt_points)))

    local lw = tw.hud.measure(label, label_size)
    local vw = tw.hud.measure(value, rating_size, "semibold")
    local ow = tw.hud.measure(alt, label_size)

    -- One baseline for three different sizes, so the small text sits on the big text's bottom edge.
    -- Exact rather than eyeballed, which is the whole reason a script can measure at all.
    local text_x = rx + (right_w - (lw + 8 + vw + ow)) * 0.5
    local baseline = ry + rating_size

    tw.hud.text(text_x, baseline - label_size, label, DIM, label_size)
    tw.hud.text(text_x + lw + 8, baseline - rating_size, value, TEXT, rating_size, "semibold")

    -- Which of the two the second figure is is never in doubt - it sits above the headline in one
    -- case and below it in the other - so it is tinted by direction rather than labelled: an upside
    -- still to be had is quiet, a Clean Finish already banked and now at risk is not.
    tw.hud.text(text_x + lw + 8 + vw, baseline - label_size, alt, grid_empty and WARN or FAINT, label_size)

    -- The rolled score, not the raw one, so the rail and the figure it is a picture of arrive
    -- together - and so a dot lights on the frame the *displayed* number crosses its threshold. A
    -- dot that lit off the true score would light while the number above it still read short of it.
    local bar_y = ry + rating_size + 8
    draw_medal_rail(rx, bar_y, rx + right_w, rail_h, medal_dot, medal_gap, roll("medal", now_points), bronze_at:get(),
        silver_at:get(), gold, MEDAL_STOPS, TRACK, fade)

    L.rx = rx
end
