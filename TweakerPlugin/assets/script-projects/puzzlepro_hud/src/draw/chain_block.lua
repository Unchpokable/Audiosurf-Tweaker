-- The chain multiplier, its drop timer and the feats, bottom-centre (DESIGN.md §4, §5, §6).

local colours = require("colours")
local chain = require("chain")
local roll = require("roll")
local palette = require("palette")
local FEAT_ICON = require("scoring").FEAT_ICON

local WHITE, COLOUR_BY_ID, YELLOW, RED = colours.WHITE, colours.COLOUR_BY_ID, colours.YELLOW, colours.RED
local CHAIN_MAX, CHAIN_FIRST = chain.MAX, chain.FIRST
local chain_bonus, drop_timer, chain_colour = chain.bonus, chain.drop_timer, chain.colour

-- What colour a feat is drawn in. Every feat in the row is one the player already holds - the row lists
-- what earned_bonus() found, plus Clean Finish while the board is clear - so the colour says "this is
-- yours" and grey said nothing.
--
-- The two colour feats take the block colour they are about from the live palette, so a player who has
-- recoloured red sees Seeing Red in their red. Stealth is the Mono feat and takes the Mono road colour.
-- The rest are literals in colours.lua; Clean Finish shares AHEAD with the gap to the personal best,
-- because both mean "you are on the good side of it". Mono Special is worth nothing and is drawn as
-- plain text. Raw colours: the caller fades them once.
local FEAT_COLOUR = {
    ["Clean Finish"] = colours.AHEAD,
    ["Match7"] = colours.MATCH7,
    ["Match11"] = colours.MATCH11,
    ["Match21"] = colours.MATCH21,
}

local function feat_colour(feat)
    if feat == "Seeing Red" then
        return COLOUR_BY_ID[RED].colour
    elseif feat == "Butter Ninja" then
        return COLOUR_BY_ID[YELLOW].colour
    elseif feat == "Stealth" then
        return palette.mono()
    end
    return FEAT_COLOUR[feat]
end

-- The rows' sizes and spacing, here because the bottom band is cut to them (band_height below).
local TIMER_SCALE = 0.62
local ROW_GAP = 4
local FEATS_GAP = 8

-- How tall the block is at its fullest - multiplier, timer, feats - from the font sizes alone. main.lua
-- makes this the height of the band the song badge and the rating share with it, and it has to be the
-- fullest height rather than this frame's: the timer row comes and goes with the chain, and a band that
-- followed it would move the badge and the rating every time.
local function band_height(L)
    local _, chain_h = tw.hud.measure("x0.0", L.chain_size, "semibold")
    local _, timer_h = tw.hud.measure("0.00", L.chain_size * TIMER_SCALE, "semibold")
    return chain_h + ROW_GAP + timer_h + FEATS_GAP + L.label_size
end

local function draw(L)
    local fade, TEXT = L.fade, L.TEXT
    local base, screen_w, label_size, chain_size = L.base, L.screen_w, L.label_size, L.chain_size
    local band_y0, band_y1, left_r, rx = L.band_y0, L.band_y1, L.left_r, L.rx
    local feats, grid_empty = L.feats, L.grid_empty

    -- ---- the chain, its drop timer and the feats, bottom-centre ------------------------------
    --
    -- Three rows on one centre, so they read as a column rather than as three things that happen to
    -- be near each other: the multiplier, the countdown to the pass that will judge it, and the
    -- feats. They share a centre and one rule - stay out of the song badge and the rating block -
    -- and nothing else. Their widths are independent on purpose.
    --
    -- The chain rails used to be sized from the feat row, which was a mistake worth recording: a
    -- feat can appear and vanish mid-run - Clean Finish flickers on every time the grid happens to
    -- empty - and the rails would jump a width the player had read as a chain reading. The two have
    -- nothing to do with each other, so they are no longer allowed to move each other.
    if grid_empty then
        table.insert(feats, 1, "Clean Finish")
    end

    local icon_size = label_size * 1.3
    local icon_gap = 5
    local feat_gap = 14

    local feats_w = 0
    for i, feat in ipairs(feats) do
        if i > 1 then
            feats_w = feats_w + feat_gap
        end
        feats_w = feats_w + icon_size + icon_gap + tw.hud.measure(feat, label_size)
    end

    -- Centred on the VIEWPORT, and kept clear of the two side blocks. Those are two different
    -- things and it used to do only the second: centring on the middle of the leftover strip put
    -- the block off-centre by half the difference between the left block's width and the rating block's,
    -- which is visible, because the eye reads "centred" against the screen and not against whatever
    -- space happened to be left over.
    --
    -- The room it gets is the NEARER of the two sides, mirrored - taking each side's own distance
    -- would let the block grow lopsided about the centre it is supposed to be on.
    local free_l = left_r + 24
    local free_r = rx - 24
    local block_cx = screen_w * 0.5
    local avail = 2 * math.min(block_cx - free_l, free_r - block_cx)

    -- One tween behind the colour, the length and the printed number, so the three cannot arrive at
    -- different times (DESIGN.md §6).
    local chain_gain = chain_bonus()
    local gain = chain_gain and roll("chain", chain_gain, 0.28) or nil
    local chain_text = gain and string.format("x%.1f", 1 + gain) or nil
    -- The timer row is a picture of what happens to the row above it, so it is not drawn without
    -- one: on its own it would be a countdown to nothing in particular.
    local timer_text, timer_frac = nil, 0
    if chain_text ~= nil then
        timer_text, timer_frac = drop_timer()
    end

    local timer_size = chain_size * TIMER_SCALE
    local rail_gap = base * 0.7
    local chain_w, chain_h = 0, 0
    local timer_w, timer_h = 0, 0
    if chain_text ~= nil then
        chain_w, chain_h = tw.hud.measure(chain_text, chain_size, "semibold")
    end
    if timer_text ~= nil then
        timer_w, timer_h = tw.hud.measure(timer_text, timer_size, "semibold")
    end

    -- One gap wide enough for both numbers, so the two rows' rails line up in a column instead of
    -- each row indenting itself by whatever its own text happens to measure.
    local gap_w = math.max(chain_w, timer_w)

    -- A rail is a sixteenth of the viewport per point of bonus. A fraction of the viewport rather
    -- than a fixed pixel count because a bar that reads well at 1280 is a smear at QHD; per point of
    -- bonus rather than of some maximum because the bonus is what the bar is a picture of, and the
    -- proportion then holds at every length instead of only at the ends.
    --
    -- Only the clamp knows about anything else on screen: whatever is left of the free strip once
    -- the number in the middle has had its space, halved between the two rails.
    local rail_base = screen_w / 16
    local rail_room = (avail - gap_w - rail_gap * 2) * 0.5
    local rail_w = math.min(rail_base * (gain or 0), rail_room)

    -- Negative when the viewport centre has been squeezed past one of the side blocks, which is a
    -- window too narrow for a centre column at all.
    if avail > 0 then
        local feats_y = band_y1 - label_size
        -- Centred in the strip and nothing more. Unlike the rails the feats have no length to give
        -- back - the row is as wide as its own words - so the only lever is where it starts, and
        -- pinning that to the left edge would not save the row, it would just decide that the
        -- rating block is the one it runs into. Centred, an overrun costs both sides equally, and
        -- it takes an unplayably narrow window and a full hand of feats to get there at all.
        local fx = block_cx - feats_w * 0.5
        for _, feat in ipairs(feats) do
            -- TEXT, already faded, for a feat with no colour of its own - Mono Special, or Stealth before
            -- the palette has been read.
            local raw = feat_colour(feat)
            local c = raw and tw.fade(raw, fade) or TEXT
            local icon = FEAT_ICON[feat]
            if icon then
                tw.hud.icon(icon, fx, feats_y + (label_size - icon_size) * 0.5, icon_size, c)
            end
            fx = fx + icon_size + icon_gap
            tw.hud.text(fx, feats_y, feat, c, label_size)
            fx = fx + tw.hud.measure(feat, label_size) + feat_gap
        end

        -- The multiplier and its drop timer are two rows on one grid: one centre, one reserved gap
        -- for the number, one pair of inner rail edges. Both rows' rails grow outward from that gap,
        -- which is what makes them shrink *towards the middle* instead of each towards its own
        -- centre.
        if chain_text ~= nil then
            local colour = chain_colour(gain)
            local rail_h = math.max(3, base * 0.18)
            local inner_l = block_cx - gap_w * 0.5 - rail_gap
            local inner_r = block_cx + gap_w * 0.5 + rail_gap

            -- The glow carries the intensity, so a long chain lights up rather than merely changing
            -- hue - which is the part that is legible out of the corner of an eye. This one is on
            -- the bonus against its maximum rather than on the rail length, because the length is
            -- open-ended and a brightness has to have a top.
            local strength = (0.3 + 0.7 * math.min(gain / CHAIN_MAX, 1)) * fade

            local function rails(mid, width, height, weight)
                -- Shorter than it is thick is a dot, not a bar. Below that the rounding eats the
                -- whole shape and what is left reads as a rendering fault.
                if width <= height then
                    return
                end
                local c = tw.fade(colour, fade)
                local radius = height * 0.5
                local y0, y1 = mid - height * 0.5, mid + height * 0.5

                tw.hud.rect(inner_l - width, y0, inner_l, y1, c, radius)
                tw.hud.glow_rect(inner_l - width, y0, inner_l, y1, c, radius, weight)
                tw.hud.rect(inner_r, y0, inner_r + width, y1, c, radius)
                tw.hud.glow_rect(inner_r, y0, inner_r + width, y1, c, radius, weight)
            end

            -- From the top of the band down, so the multiplier stays put whether or not the timer row
            -- is there under it.
            local chain_y = band_y0
            local timer_y = chain_y + chain_h + ROW_GAP

            rails(chain_y + chain_h * 0.5, rail_w, rail_h, strength)
            -- The number stays white at every chain length: the rails carry the colour, and two
            -- things changing colour together is one thing too many.
            tw.hud.text(block_cx - chain_w * 0.5, chain_y, chain_text, tw.fade(WHITE, fade), chain_size, "semibold")

            -- The countdown is only shown while there is a chain to lose. The collection pass runs
            -- either way, but with no chain the number is a countdown to nothing in particular, and
            -- read on its own - with no rails beside it and a multiplier of x1.0 above it - it looks
            -- like a counter of something unexplained.
            --
            -- Held, not switched: this rises to full over the same roll that grows the rails, so
            -- starting a chain brings the row up with them and dropping one takes it away with them.
            -- The first envelope key is where a chain is worth anything at all, so that is where it
            -- reaches full weight.
            local held = math.min(gain / CHAIN_FIRST, 1)
            if timer_text ~= nil and held > 0.01 then
                -- The chain's own rails scaled by the time left, so the row below is literally a
                -- picture of how much of the row above survives if nothing matches.
                rails(timer_y + timer_h * 0.5, rail_w * timer_frac, math.max(2, rail_h * 0.7), strength * 0.7)
                tw.hud.text(block_cx - timer_w * 0.5, timer_y, timer_text, tw.fade(tw.alpha(WHITE, held), fade), timer_size,
                    "semibold")
            end
        end
    end
end

return {
    draw = draw,
    band_height = band_height,
}
