-- The colours and colour ids everything else agrees on: the block colours and how the game buckets
-- them, the literal medal and ahead/behind colours, and mix() to blend two of them.
--
-- COLOURS[i].colour is filled from the game's live palette by palette.lua; until then it is the
-- fallback below.

-- One row per bucket of BUCKET below, keyed by the colour id the palette should draw for it.
local COLOURS = {
    { id = 0, name = "purple", fallback = 0xFFE08CC8 },
    { id = 1, name = "blue", fallback = 0xFFE0C060 },
    { id = 2, name = "green", fallback = 0xFF60C060 },
    { id = 3, name = "yellow", fallback = 0xFF40D0E0 },
    { id = 4, name = "red", fallback = 0xFF5050E0 },
    -- "White" is the game's word for the row, not a description of what is in it: the switch below
    -- puts both Wild (6) and Stone (7) here. The swatch shows Stone, which is the common one.
    { id = 7, name = "white", fallback = 0xFFE0E0E0 },
}

-- Raw colour id -> the row it belongs to. This is ChannelSwitch #1248 in
-- Achievements::Do_GetTrafficCounts, read off the graph case by case:
--
--     0 -> PurpleTotal   1 -> BlueTotal   2 -> GreenTotal   3 -> YellowTotal
--     4 -> RedTotal      5 -> RedTotal    6 -> WhiteTotal   7 -> WhiteTotal
--
-- and no case 8 or above at all. An earlier version of this script had "7,8 white, 6 counted by
-- nobody", which is wrong at both ends: **Wild (6) counts as white** and **Bomb (8) is counted by
-- nobody**. That put every wild block on the track outside the denominator and every bomb inside
-- it.
--
-- Worth knowing but not fixable here: the game's *hit* switch (#1123 in Do_ReportBlockHit) does not
-- agree with this one. It maps 5 -> WildsUsed and 6 -> WhitesHit, and has no case 7 at all. So a
-- track carrying id-5 blocks has them in RedTotal but never in RedsHit, and the game's own Seeing
-- Red test cannot reach 95% on it. Predicting from the same two channels the game tests means
-- inheriting that, which is correct: the job is to predict what the game will do.
local BUCKET = { [0] = 0, [1] = 1, [2] = 2, [3] = 3, [4] = 4, [5] = 4, [6] = 7, [7] = 7 }

-- Bucket id -> its row, so a raw ColorID off a score event reaches a colour in two lookups instead
-- of a scan. Built rather than written out because COLOURS is the source of truth for which rows
-- exist, and a second hand-kept list of the same ids is a second thing to get wrong.
local COLOUR_BY_ID = {}
for _, c in ipairs(COLOURS) do
    COLOUR_BY_ID[c.id] = c
end

local YELLOW, RED = 3, 4

-- Medal colours are semantic rather than chrome, so they stay literal; everything else comes from
-- the overlay's palette so the widget belongs to Tweaker rather than merely sitting on top of it.
local BRONZE = 0xFF4E7DCD
local SILVER = 0xFFC8C8C8
local GOLD = 0xFF3FD5F5
local WHITE = 0xFFFFFFFF

-- Ahead of / behind the personal best, and the colour a lost point is drawn in. Literal for the
-- same reason the medals are: "you are winning" and "that cost you" are meanings, not chrome, and a
-- theme that recoloured them would be recolouring the sentence.
local AHEAD = 0xFF70D890
local BEHIND = 0xFF4848E0

-- The Match feats, light to deep in the order they are earned, so a bigger match reads as a stronger
-- blue rather than as a different thing. Blue because none of the three is about a block colour, and
-- the block blue is the one the palette cannot be relied on to keep (Pro folds green into it, Mono
-- replaces it outright) - these are literal for the same reason the medals are.
local MATCH7 = 0xFFE6CDA0
local MATCH11 = 0xFFF0C36E
local MATCH21 = 0xFFFFB432

-- In tier order, which is also the order of the three marker dots under the medal rail.
local MEDALS = { BRONZE, SILVER, GOLD }
-- Built once: these are roll() keys, looked up every frame, and building them by concatenation in
-- the draw loop would put three throwaway strings a frame on the collector for no reason.
local DOT_KEYS = { "medal_dot_bronze", "medal_dot_silver", "medal_dot_gold" }

for _, c in ipairs(COLOURS) do
    c.colour = c.fallback
    -- Built once rather than concatenated per frame per cell: twelve throwaway strings a frame is
    -- twelve more than a table of six fixed rows needs.
    c.key_total = "total:" .. c.name
    c.key_taken = "taken:" .. c.name
end

local function mix(a, b, t)
    if t <= 0 then
        return a
    end
    if t >= 1 then
        return b
    end

    local out = 0
    local scale = 1
    for _ = 1, 4 do
        local ca = a % 256
        local cb = b % 256
        out = out + math.floor(ca + (cb - ca) * t + 0.5) * scale
        a = math.floor(a / 256)
        b = math.floor(b / 256)
        scale = scale * 256
    end
    return out
end

return {
    COLOURS = COLOURS,
    BUCKET = BUCKET,
    COLOUR_BY_ID = COLOUR_BY_ID,
    YELLOW = YELLOW,
    RED = RED,
    BRONZE = BRONZE,
    SILVER = SILVER,
    GOLD = GOLD,
    WHITE = WHITE,
    AHEAD = AHEAD,
    BEHIND = BEHIND,
    MATCH7 = MATCH7,
    MATCH11 = MATCH11,
    MATCH21 = MATCH21,
    MEDALS = MEDALS,
    DOT_KEYS = DOT_KEYS,
    mix = mix,
}
