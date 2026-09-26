-- Text helpers shared by the drawing code.

-- A score with its thousands spaced out. Six- and seven-figure numbers are the normal case here and
-- an unbroken run of digits is not readable at a glance, which is the only thing this number is for.
-- A plain space rather than a narrow one: the bundled faces are not guaranteed to carry U+2009, and
-- a missing glyph would be a box in the middle of the score.
local function grouped(n)
    local s = string.format("%.0f", math.abs(n))
    local sign = (n < 0) and "-" or ""

    local out = s:sub(-3)
    local at = #s - 3
    while at > 0 do
        local from = math.max(1, at - 2)
        out = s:sub(from, at) .. " " .. out
        at = from - 1
    end

    return sign .. out
end

local function centered(x, width, y, text, colour, size, font)
    local w = tw.hud.measure(text, size, font)
    tw.hud.text(x + (width - w) * 0.5, y, text, colour, size, font)
end

return {
    grouped = grouped,
    centered = centered,
}
