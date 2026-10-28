-- One line of text in a box narrower than it may be (DESIGN.md §12): drawn as it is when it fits, and when
-- it does not, either cut with an ellipsis or scrolled - it rests at its start, slides along to show the
-- rest, and comes round to the start again without a jump, because a second copy follows the first.
--
-- The scrolling is done glyph by glyph rather than with a clip rectangle, and that is not a workaround:
-- the edges are soft - a glyph fades out as it nears the edge of the box - and a clip gives a hard edge
-- that cuts letters in half. Fading needs each glyph drawn on its own anyway. The text is split and
-- measured once, when it or its size changes: the glyph strings and their offsets are kept, so a frame
-- of scrolling allocates nothing.
--
-- Offsets come from measuring prefixes of the text, not from adding up single glyphs, so they are
-- where the text engine would put each glyph in the whole string.

local fitted = {}

-- How fast the text scrolls, in its own heights per second, and the space between the end of the text
-- and the copy that follows it, in the same unit.
local SPEED = 2.5
local GAP = 3
-- How far from the edge of the box a glyph starts to fade, in text heights.
local FADE = 1.0

local ELLIPSIS = "\226\128\166" -- U+2026, which the overlay's faces carry

-- A line's state. One per line the caller draws, kept across frames.
function fitted.new()
    return { text = nil, size = nil, font = nil, glyphs = {}, at = {}, width = 0, height = 0, clock = 0, cut = nil, cut_for = nil }
end

local function prepare(line, text, size, font)
    if line.text == text and line.size == size and line.font == font then
        return
    end
    line.text, line.size, line.font = text, size, font
    line.clock = 0
    line.cut, line.cut_for = nil, nil

    local glyphs, at = {}, {}
    -- A UTF-8 sequence: one lead byte (ASCII, or 0xC2-0xF4) and its continuation bytes. LuaJIT has no
    -- utf8 library, and this is all of it that is needed.
    for g in text:gmatch("[%z\1-\127\194-\244][\128-\191]*") do
        glyphs[#glyphs + 1] = g
    end
    local prefix = ""
    for i, g in ipairs(glyphs) do
        at[i] = tw.hud.measure(prefix, size, font)
        prefix = prefix .. g
    end
    line.glyphs, line.at = glyphs, at
    line.width, line.height = tw.hud.measure(text, size, font)
    -- The end of the last glyph is the whole width.
    at[#glyphs + 1] = line.width
end

-- The longest start of the text that fits in `max_w` with an ellipsis after it.
local function cut(line, max_w)
    if line.cut_for == max_w then
        return line.cut
    end
    local room = max_w - tw.hud.measure(ELLIPSIS, line.size, line.font)
    local n = 0
    while n < #line.glyphs and line.at[n + 2] <= room do
        n = n + 1
    end
    line.cut = table.concat(line.glyphs, "", 1, n):gsub("%s+$", "") .. ELLIPSIS
    line.cut_for = max_w
    return line.cut
end

-- The width the line takes, which is never more than `max_w`. Measuring is drawing's first step, so a
-- caller sizing a box around the line calls measure() first and draw() after.
function fitted.measure(line, text, size, font, max_w)
    prepare(line, text, size, font)
    return math.min(line.width, max_w), line.height
end

-- `mode` is "scroll" or "ellipsis"; `pause` the seconds a scrolling line rests at its start.
function fitted.draw(line, x, y, max_w, colour, mode, pause)
    if line.width <= max_w then
        tw.hud.text(x, y, line.text, colour, line.size, line.font)
        return
    end

    if mode == "ellipsis" then
        tw.hud.text(x, y, cut(line, max_w), colour, line.size, line.font)
        return
    end

    local size = line.size
    local gap = size * GAP
    local loop = line.width + gap
    local speed = size * SPEED
    line.clock = line.clock + tw.dt()
    local offset = math.max(0, line.clock - pause) * speed
    if offset >= loop then
        line.clock, offset = 0, 0
    end

    -- The right edge fades by a glyph's far side, so nothing is ever drawn past it. The left edge fades
    -- by a glyph's centre - a glyph on its way out may hang half over the edge, invisible by then - and
    -- only comes in as the text starts to move: at rest the line starts where a line starts, and a first
    -- letter faded into the box would look like a rendering fault, while switching the fade on at once
    -- would make that letter vanish on the first frame of scrolling.
    local fade = size * FADE
    local left_in = math.min(1, offset / fade)
    local x1 = x + max_w
    local glyphs, at = line.glyphs, line.at
    for copy = 0, 1 do
        local base = x + copy * loop - offset
        for i = 1, #glyphs do
            local gx0 = base + at[i]
            local gx1 = base + at[i + 1]
            if gx1 > x and gx0 < x1 then
                local right = math.min(1, (x1 - gx1) / fade)
                local left = math.min(math.max(((gx0 + gx1) * 0.5 - x) / fade, 0), 1)
                local a = math.min(right, 1 - left_in * (1 - left))
                if a > 0.01 then
                    tw.hud.text(gx0, y, glyphs[i], tw.fade(colour, a), size, line.font)
                end
            end
        end
    end
end

return fitted
