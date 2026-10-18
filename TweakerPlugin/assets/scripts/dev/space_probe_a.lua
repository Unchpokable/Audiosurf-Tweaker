-- @name        Space Probe A
-- @description Places two cards in the top-right corner, one blinking - pair it with Space Probe B
-- @api         2
--
-- A probe for sharing the HUD (Docs/Internal/script-coexistence.md §4), not a script anyone wants
-- running while they play. Copy it and space_probe_b.lua flat into Scripts\.
--
-- A puts two cards at the top-right corner with tw.hud.place: the first always, the second blinking once
-- a second. While the second is hidden, A claims the spot it was last drawn at with tw.hud.claim (CLAIM
-- below), so B - placed after A in the run order - stays put instead of jumping up into the gap and back.
-- Set CLAIM = false and Reload to see the jump that claim exists to prevent. Put B above A in the Scripts
-- tab, and B's card takes the corner.

local CLAIM = true
local W, H = 240, 48
local TINT = 0xFF3C78F0

local function card(x, y, text)
    tw.hud.rect(x, y, x + W, y + H, tw.alpha(TINT, 0.92), 8)
    tw.hud.text(x + 12, y + 14, text, tw.theme("text_primary"))
end

-- Where the blinking card was last drawn, for claiming while it is hidden.
local blink_x, blink_y = nil, nil

tw.on_frame(function()
    local x, y = tw.hud.place(W, H, "top_right")
    if x then card(x, y, "Space Probe A - always") end

    local shown = math.floor(tw.draw_frame_count / 60) % 2 == 0
    if shown then
        x, y = tw.hud.place(W, H, "top_right")
        if x then
            card(x, y, "Space Probe A - blinking")
            blink_x, blink_y = x, y
        end
    elseif CLAIM and blink_x then
        tw.hud.claim(blink_x, blink_y, blink_x + W, blink_y + H)
    end
end)
