-- @name        Order Probe B
-- @description Shows where it runs in the script order - pair it with Order Probe A
--
-- A probe for the run order (Docs/Internal/script-coexistence-roadmap.md §3), not a script anyone
-- wants running while they play. Copy it and order_probe_a.lua flat into Scripts\ and open the
-- overlay's Scripts tab.
--
-- Both draw a card at almost the same spot, B offset a little down and to the right. The one drawn
-- later is on top, and each card says which of the two it was this frame, in drawing (on_frame) and in
-- computing (on_tick). Drag either row in the Scripts tab and both numbers swap at once; Reload either
-- script and they stay as they were - that is the point of the order belonging to the script and not
-- to the moment it registered.
--
-- The two learn about each other through a table on _G, which is how this probe can tell who went
-- first. Real scripts have no business sharing state like this.

local ME, SHIFT, TINT = "B", 24, 0xFFE0904A

local function slot(key, stamp)
    local seen = rawget(_G, key)
    if seen == nil or seen.stamp ~= stamp then
        seen = { stamp = stamp, count = 0 }
        rawset(_G, key, seen)
    end
    seen.count = seen.count + 1
    return seen.count
end

local ticked = 0

tw.on_tick(function()
    ticked = slot("__order_probe_tick", tw.frame)
end)

tw.on_frame(function()
    local drawn = slot("__order_probe_frame", tw.draw_frame_count)

    local x0, y0 = tw.hud.safe()
    x0, y0 = x0 + 40 + SHIFT, y0 + 220 + SHIFT
    local x1, y1 = x0 + 300, y0 + 64

    tw.hud.rect(x0, y0, x1, y1, tw.alpha(TINT, 0.92), 8)
    tw.hud.rect(x0, y0, x1, y1, tw.theme("border"), 8, 1)
    tw.hud.text(x0 + 12, y0 + 10, "Order Probe " .. ME, tw.theme("text_primary"))
    tw.hud.text(x0 + 12, y0 + 34,
        string.format("drawn %d of 2, ticked %d of 2", drawn, ticked), tw.theme("text_primary"))
end)
