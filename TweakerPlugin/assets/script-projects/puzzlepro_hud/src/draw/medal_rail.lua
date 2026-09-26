-- The medal rail under the skill rating: progress to gold as one hairline, and three dots for the
-- medals held.

local colours = require("colours")
local roll = require("roll")

local MEDALS, DOT_KEYS, mix = colours.MEDALS, colours.DOT_KEYS, colours.mix

-- Which medal the score currently holds: 0 none, 1 bronze, 2 silver, 3 gold. The game's own test
-- (Do_CalculateMedalEarned #845) is three unordered comparisons against PointsWithGridBonus, which
-- is what the caller passes; a threshold the script could not read is skipped rather than assumed.
local function medal_tier(score, bronze, silver, gold)
    if gold ~= nil and gold > 0 and score >= gold then
        return 3
    end
    if silver ~= nil and silver > 0 and score >= silver then
        return 2
    end
    if bronze ~= nil and bronze > 0 and score >= bronze then
        return 1
    end
    return 0
end

-- Progress to gold, in the same language as the chain rails: a hairline, one colour, and light.
--
-- The rail carries ONE colour - the medal currently held - rather than three abutting zones. At the
-- thickness the rest of this HUD uses, three colours in a row do not read as three zones; they read
-- as a smear, which is exactly what the old bar had become. So the colour is the medal, the length
-- is the progress, and the three dots underneath are the discrete state.
--
-- The dots are indicators, not dividers. They sit at equal spacing, centred under the rail, and
-- deliberately say nothing about where the thresholds fall - putting them on the rail at their
-- score positions is what made the old bar look like a chart.
--
-- There is deliberately no overshoot indicator either. Gold is PointsWithGridBonus ==
-- GoldRequirement, so by Do_CalcSkillRating (DESIGN.md §7) gold is exactly 100 skill rating per league:
-- 100 casual, 200 pro, 300 elite. Anyone past gold is already reading that off the number, and a
-- second thing saying the same is a second thing to keep true.
local function draw_medal_rail(x0, y0, x1, h, dot, gap_y, score, bronze, silver, gold, stops, track, fade)
    local radius = h * 0.5
    local w = x1 - x0

    tw.hud.rect(x0, y0, x1, y0 + h, track, radius)

    -- The tier is tweened and the colour interpolated along the stops by that rolled ordinal, so
    -- earning a medal sweeps the rail from one metal to the next instead of switching it. Same trick
    -- as the chain gradient: one number moving, not two colours picked per frame.
    local tier = medal_tier(score, bronze, silver, gold)
    local t = roll("medal_tier", tier, 0.35)
    local lo = math.min(math.floor(t), #stops - 2)
    local colour = mix(stops[lo + 1], stops[lo + 2], t - lo)

    if gold ~= nil and gold > 0 then
        local fill = w * math.min(math.max(score / gold, 0), 1)
        -- Shorter than it is thick is a dot, not a bar - the same rule the chain rails use, and for
        -- the same reason: below that the rounding eats the shape.
        if fill > h then
            local c = tw.fade(colour, fade)
            tw.hud.rect(x0, y0, x0 + fill, y0 + h, c, radius)
            -- The glow rises with the tier, so the rail gains weight as the medals do rather than
            -- only changing hue.
            tw.hud.glow_rect(x0, y0, x0 + fill, y0 + h, c, radius, (0.3 + 0.2 * t) * fade)
        end
    end

    local spacing = math.max(dot * 2.5, w * 0.1)
    local cy = y0 + h + gap_y
    for i, medal in ipairs(MEDALS) do
        -- Lit is its own roll per dot rather than a slice of the tier roll: a medal lights up on its
        -- own clock, and sharing one would make the third dot start moving when the first was won.
        local lit = roll(DOT_KEYS[i], tier >= i and 1 or 0, 0.35)
        local cx = x0 + w * 0.5 + spacing * (i - 2)
        -- Unearned is the medal's own colour held down rather than a neutral, so the row reads as
        -- three named medals waiting rather than as three anonymous pips.
        local c = tw.fade(tw.alpha(medal, 0.16 + 0.84 * lit), fade)
        tw.hud.rect(cx - dot * 0.5, cy, cx + dot * 0.5, cy + dot, c, dot * 0.5)
        if lit > 0.01 then
            tw.hud.glow_rect(cx - dot * 0.5, cy, cx + dot * 0.5, cy + dot, c, dot * 0.5, 0.75 * lit * fade)
        end
    end
end

return draw_medal_rail
