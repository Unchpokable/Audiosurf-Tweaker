-- The colour table: a swatch per colour the track carries, the track's total of it, and how much of it
-- has been taken so far. Drawn into a box it is given - where that box sits is the stats panel's
-- business (draw/stats_panel.lua).

local scan = require("scan")
local tally = require("tally")
local roll = require("roll")
local centered = require("draw.text").centered

local totals, taken = scan.totals, tally.taken

-- How tall the table is. Fixed by the sizes alone, so the panel around it can be laid out before
-- anything in it is drawn.
local function height(L)
    return L.swatch + L.row_gap + L.row_h * 2
end

-- `width` is shared out evenly between the columns, however many the track has. The panel keeps one
-- width for every mode, so a Mono track's two colours sit in wide cells rather than the table - and the
-- minimap under it - shrinking to a third of the panel.
local function draw(L, x, y, width)
    local fade, RULE, TEXT, DIM, FAINT = L.fade, L.RULE, L.TEXT, L.DIM, L.FAINT
    local value_size, swatch, row_gap, row_h = L.value_size, L.swatch, L.row_gap, L.row_h

    -- Only the colours this track actually carries (see visible_columns in scan.lua): a Casual run has three,
    -- Pro four, Mono two, and a column of dashes for a colour that cannot appear is noise.
    local columns = totals.columns
    local cell_w = width / #columns
    local swatch_y = y
    local totals_y = swatch_y + swatch + row_gap
    local taken_y = totals_y + row_h

    tw.hud.line(x, totals_y - 2, x + width, totals_y - 2, RULE, 1)
    tw.hud.line(x, taken_y - 2, x + width, taken_y - 2, RULE, 1)

    for i, c in ipairs(columns) do
        local cx = x + cell_w * (i - 1)
        local total = totals.ready and totals.by_row[c.id] or nil
        local got = taken[c.id] or 0

        local colour = tw.fade(c.colour, fade)

        -- A small square with a soft halo rather than a bar filling the cell: at a glance the row
        -- reads as six markers over six numbers, not as a stacked chart.
        local sx = cx + (cell_w - swatch) * 0.5
        tw.hud.rect(sx, swatch_y, sx + swatch, swatch_y + swatch, colour, 3)
        tw.hud.glow_rect(sx, swatch_y, sx + swatch, swatch_y + swatch, colour, 3, 0.55 * fade)

        -- Both numbers roll to their new value rather than snapping to it. The rolled figure is for
        -- display only - every decision below (is there a total at all, is the colour finished) is
        -- made on the real one, so a colour does not light up early because its counter is still
        -- catching up.
        local shown_total = total and math.floor(roll(c.key_total, total) + 0.5)
        local shown_taken = math.floor(roll(c.key_taken, got) + 0.5)

        if total == nil or total < 1 then
            centered(cx, cell_w, totals_y + 3, "--", FAINT, value_size, "semibold")
            centered(cx, cell_w, taken_y + 3, total == nil and "--" or tostring(shown_taken), total == nil and FAINT or DIM,
                value_size, "semibold")
        else
            centered(cx, cell_w, totals_y + 3, tostring(shown_total), DIM, value_size, "semibold")
            -- A colour finished off the road reads as that colour rather than as plain text.
            centered(cx, cell_w, taken_y + 3, tostring(shown_taken), got >= total and colour or TEXT, value_size, "semibold")
        end
    end
end

return {
    height = height,
    draw = draw,
}
