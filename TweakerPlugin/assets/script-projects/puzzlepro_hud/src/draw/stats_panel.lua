-- The stats panel, top-left: the colour table, and under it the minimap. One panel rather than two widgets
-- because both are about the track as a whole - what it carries and where you are on it - and the
-- corner is where the game kept its own minimap.

local roll = require("roll")
local colour_table = require("draw.colour_table")
local draw_minimap = require("draw.minimap")

-- The widest the table gets: purple, blue, green, yellow, red and white. The panel is always this wide,
-- whatever the mode, so it does not change size between a Casual run and a Mono one.
local MAX_COLUMNS = 6

-- The map's height as a share of its width. The game's own is 2:1; a profile this flat is still easy to
-- read at a glance, and it keeps the panel from reaching down over the track.
local MAP_ASPECT = 0.26

-- Whether the time left is printed under the map. The settings tab writes it.
local settings = { time_left = true }

-- "2:31". Whole seconds, rounded up, so it reaches 0:00 as the song ends and not a second before.
local function clock(seconds)
    local s = math.ceil(seconds)
    return string.format("%d:%02d", math.floor(s / 60), s % 60)
end

local function draw(L)
    local fade, base, margin, label_size, DIM = L.fade, L.base, L.margin, L.label_size, L.DIM

    local pad = math.max(8, base * 0.6)
    local inner_w = L.cell_w * MAX_COLUMNS
    local table_h = colour_table.height(L)
    local map_gap = pad
    local map_h = math.floor(inner_w * MAP_ASPECT + 0.5)
    local _, clock_h = tw.hud.measure("0:00", label_size)

    local w = inner_w + pad * 2
    local h = table_h + map_gap + map_h + clock_h + pad * 2
    local x0 = L.sx0 + margin

    -- Measured from the top of the VIEWPORT, as the score strip is, not from the safe area: the safe area's
    -- top band is as tall as the whole notification column, and honouring it would park the panel a good
    -- way down the screen for the sake of a feed that is empty most of the time.
    --
    -- When the feed does have something in it and its box crosses the panel's, the panel moves down under
    -- it - rolled, so a toast arriving slides the panel rather than throwing it, and the panel comes back
    -- the same way when the toast goes.
    local top = margin
    local nx0, ny0, nx1, ny1 = tw.hud.widget("notefeed")
    if nx0 ~= nil and ny1 > ny0 and nx0 < x0 + w and nx1 > x0 and ny0 < top + h then
        top = ny1 + 8
    end
    local y0 = roll("stats_panel_y", top, 0.3)

    -- The same backdrop as the score strip, and for the same reason: white numbers on a bright sky are
    -- not numbers. A panel rather than a band with falloff, because this sits in a corner and has edges.
    -- Its height is fixed, map or no map, so it does not grow when a track finishes loading.
    local back = tw.fade(tw.alpha(tw.theme("surface"), 0.72), fade)
    tw.hud.rect(x0, y0, x0 + w, y0 + h, back, 6)

    local ix = x0 + pad
    colour_table.draw(L, ix, y0 + pad, inner_w)

    local map_y = y0 + pad + table_h + map_gap
    local left = draw_minimap(L, ix, map_y, inner_w, map_h)
    if left ~= nil and settings.time_left then
        local text = clock(left)
        local text_w = tw.hud.measure(text, label_size)
        tw.hud.text(ix + inner_w - text_w, map_y + map_h, text, DIM, label_size)
    end
end

return {
    draw = draw,
    settings = settings,
}
