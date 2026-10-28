-- The song badge, bottom-left in the band it shares with the chain and the rating (DESIGN.md §12): the
-- Tweaker's logo in a ring, the title over a hairline, the artist under it. It replaces the game's own
-- song title (muted, DESIGN.md §3).

local song = require("song")
local fitted = require("draw.fitted_text")
local chain_block = require("draw.chain_block")

-- One state per line, kept across frames: the glyphs a scrolling title is drawn from, and its clock.
local title_line = fitted.new()
local artist_line = fitted.new()

-- What a title too long for its room does - "scroll" or "ellipsis" - and how many seconds a scrolling
-- one rests at its start. The settings tab writes these.
local settings = { overflow = "scroll", pause = 6 }

local function draw(L)
    L.left_r = L.sx0 + L.margin

    local title = song.title
    if title == nil or title == "" then
        return
    end

    local fade, base, margin = L.fade, L.base, L.margin
    local TEXT, DIM, FAINT = L.TEXT, L.DIM, L.FAINT
    local band_y0, band_y1 = L.band_y0, L.band_y1
    local band_h = band_y1 - band_y0

    local title_size = base * 1.2
    local artist_size = L.label_size
    local pad = math.max(10, base * 0.7)
    local rule_gap = math.max(2, base * 0.2)

    -- Natural sizes first: the box is sized to the text, and only cut down to the room there is.
    local artist = song.artist
    local has_artist = artist ~= nil and artist ~= ""
    local _, title_h = fitted.measure(title_line, title, title_size, "semibold", math.huge)
    local artist_w, artist_h = 0, 0
    if has_artist then
        artist_w, artist_h = fitted.measure(artist_line, artist, artist_size, nil, math.huge)
    end
    local block_h = title_h + rule_gap + 1 + (has_artist and rule_gap + artist_h or 0)
    local logo_d = math.min(block_h, band_h - pad)

    -- The box is as tall as the band - the chain block at its fullest - so the three blocks along the
    -- bottom read as one row (DESIGN.md §13).
    local x0 = L.sx0 + margin
    local text_dx = pad + logo_d + pad * 0.8

    -- As wide as its text, up to where the chain block would reach with the chain at its maximum: a long
    -- title is cut or scrolled rather than allowed to shorten the bar the player reads by its length.
    local max_r = L.screen_w * 0.5 - chain_block.max_half_width(L) - 24
    local natural_w = math.max(title_line.width, artist_w)

    -- Pins can float on the left at the bottom; the badge moves out from under them, as the rating does
    -- on the right.
    local px0, py0, px1, py1 = tw.hud.widget("pins")
    if px0 and px0 < x0 + text_dx + natural_w + pad and px1 > x0 and py1 > band_y0 and py0 < band_y1 then
        x0 = px1 + 12
    end

    local text_x = x0 + text_dx
    local room = max_r - pad - text_x
    -- Too narrow a window for a badge at all: better none than a ring with three letters beside it.
    if room < base * 4 then
        return
    end
    local text_w = math.min(natural_w, room)
    local x1 = text_x + text_w + pad

    local back = tw.fade(tw.alpha(tw.theme("surface"), 0.72), fade)
    tw.hud.rect(x0, band_y0, x1, band_y1, back, 6)

    -- The logo in a ring. The ring is drawn here rather than being part of the icon, so the icon stays
    -- the plain logo every other use of it wants.
    local cy = (band_y0 + band_y1) * 0.5
    local cx = x0 + pad + logo_d * 0.5
    local r = logo_d * 0.5
    tw.hud.rect(cx - r, cy - r, cx + r, cy + r, FAINT, r, 1.5)
    local icon = logo_d * 0.55
    tw.hud.icon("tweaker_logo", cx - icon * 0.5, cy - icon * 0.5, icon, TEXT)

    local ty = cy - block_h * 0.5
    fitted.draw(title_line, text_x, ty, text_w, TEXT, settings.overflow, settings.pause)

    local uy = ty + title_h + rule_gap
    tw.hud.line(text_x, uy, text_x + math.min(title_line.width, text_w), uy, FAINT, 1)

    if has_artist then
        fitted.draw(artist_line, text_x, uy + 1 + rule_gap, text_w, DIM, settings.overflow, settings.pause)
    end

    L.left_r = x1
end

return {
    draw = draw,
    settings = settings,
}
