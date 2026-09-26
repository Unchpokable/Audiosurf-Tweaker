-- The chain multiplier, its drop timer and the feats, bottom-centre (DESIGN.md §4, §5, §6).

local colours = require("colours")
local chain = require("chain")
local roll = require("roll")
local FEAT_ICON = require("scoring").FEAT_ICON

local WHITE = colours.WHITE
local CHAIN_MAX, CHAIN_FIRST = chain.MAX, chain.FIRST
local chain_bonus, drop_timer, chain_colour = chain.bonus, chain.drop_timer, chain.colour

return function(L)
    local fade, DIM = L.fade, L.DIM
    local base, screen_w, label_size, chain_size, bottom = L.base, L.screen_w, L.label_size, L.chain_size, L.bottom
    local tx, table_w, rx = L.tx, L.table_w, L.rx
    local feats, grid_empty = L.feats, L.grid_empty

    -- ---- the chain, its drop timer and the feats, bottom-centre ------------------------------
    --
    -- Three rows on one centre, so they read as a column rather than as three things that happen to
    -- be near each other: the multiplier, the countdown to the pass that will judge it, and the
    -- feats. They share a centre and one rule - stay out of the colour table and the rating block -
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
    -- the block off-centre by half the difference between the table's width and the rating block's,
    -- which is visible, because the eye reads "centred" against the screen and not against whatever
    -- space happened to be left over.
    --
    -- The room it gets is the NEARER of the two sides, mirrored - taking each side's own distance
    -- would let the block grow lopsided about the centre it is supposed to be on.
    local free_l = tx + table_w + 24
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

    local timer_size = chain_size * 0.62
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
        local feats_y = bottom - label_size
        -- Centred in the strip and nothing more. Unlike the rails the feats have no length to give
        -- back - the row is as wide as its own words - so the only lever is where it starts, and
        -- pinning that to the left edge would not save the row, it would just decide that the
        -- rating block is the one it runs into. Centred, an overrun costs both sides equally, and
        -- it takes an unplayably narrow window and a full hand of feats to get there at all.
        local fx = block_cx - feats_w * 0.5
        for _, feat in ipairs(feats) do
            local icon = FEAT_ICON[feat]
            if icon then
                tw.hud.icon(icon, fx, feats_y + (label_size - icon_size) * 0.5, icon_size, DIM)
            end
            fx = fx + icon_size + icon_gap
            tw.hud.text(fx, feats_y, feat, DIM, label_size)
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

            local rows_h = chain_h + (timer_text ~= nil and timer_h + 4 or 0)
            local chain_y = feats_y - rows_h - 8
            local timer_y = chain_y + chain_h + 4

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
