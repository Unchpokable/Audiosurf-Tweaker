-- @name        Space Probe B
-- @description Places a card in the top-right corner and outlines everything it sees taken - pair it with Space Probe A
-- @api         2
--
-- A probe for sharing the HUD (Docs/Internal/script-coexistence.md §4), not a script anyone wants
-- running while they play. Copy it and space_probe_a.lua flat into Scripts\.
--
-- B asks tw.hud.occupied what is taken before it draws, outlines each box with its owner's name - the
-- overlay's chrome, and Space Probe A's cards when A runs above B - then puts its own card in the
-- top-right corner with tw.hud.place, which lands below A's. Move B above A in the Scripts tab and the
-- outlines of A's cards disappear: B runs first now, and A has not drawn yet when B looks.

local SHOW_BOXES = true
local W, H = 240, 48
local TINT = 0xFFE0904A

tw.on_frame(function()
    -- Asked before drawing anything, and the card placed before the outlines are drawn: what this script
    -- draws counts as taken for its own later place() calls, and the outlines should not push the card.
    local taken = tw.hud.occupied()

    local x, y = tw.hud.place(W, H, "top_right")
    if x then
        tw.hud.rect(x, y, x + W, y + H, tw.alpha(TINT, 0.92), 8)
        tw.hud.text(x + 12, y + 14, string.format("Space Probe B - sees %d box(es)", #taken), tw.theme("text_primary"))
    end

    if SHOW_BOXES then
        local line = 0xFF00E0FF -- yellow: visible over anything the overlay or the game draws
        for _, box in ipairs(taken) do
            tw.hud.rect(box.x0, box.y0, box.x1, box.y1, line, 4, 1)
            tw.hud.text(box.x0 + 4, box.y1 + 2, box.by, line, 12)
        end
    end
end)
