-- ---------------------------------------------------------------------------------------------
-- The chain multiplier
-- ---------------------------------------------------------------------------------------------

local colours = require("colours")
local ch = require("channels")

local COLOURS, WHITE, mix = colours.COLOURS, colours.WHITE, colours.mix
local chain_count, chain_curve, match_timer, match_ticks = ch.chain_count, ch.chain_curve, ch.match_timer, ch.match_ticks

-- Quest3D counts in 25ths of a second, and the game converts with a bare `/25` everywhere it wants
-- seconds. This is that 25.
local TICKS_PER_SECOND = 25

-- Envelope #643's keys, lifted out of its ENV1 chunk. Kept here for two reasons: as the fallback
-- when the channel cannot be read, and as a running cross-check on the channel - an index is the
-- most fragile way to address anything, and a wrong one here would show a plausible wrong number
-- rather than failing.
local CHAIN_KEYS = { { 0, 0.0 }, { 1, 0.5 }, { 4, 1.5 }, { 10, 2.0 }, { 20, 2.5 }, { 100, 3.0 } }

-- A full bar. The game divides by this same 3 to scale its own (DESIGN.md §4), so a rail drawn at
-- bonus/3 of its maximum is the length the player already knows how to read.
local CHAIN_MAX = CHAIN_KEYS[#CHAIN_KEYS][2]
-- The first key worth anything: a chain of 1 is a bonus of 0.5. Below it the player has no chain,
-- which is what the drop timer's visibility hangs on.
local CHAIN_FIRST = CHAIN_KEYS[2][2]

local function chain_bonus_from_table(chain)
    if chain <= 0 then
        return 0
    end

    for i = 1, #CHAIN_KEYS - 1 do
        local x0, y0 = CHAIN_KEYS[i][1], CHAIN_KEYS[i][2]
        local x1, y1 = CHAIN_KEYS[i + 1][1], CHAIN_KEYS[i + 1][2]
        if chain <= x1 then
            return y0 + (y1 - y0) * (chain - x0) / (x1 - x0)
        end
    end

    return CHAIN_KEYS[#CHAIN_KEYS][2]
end

-- Starts trusting the channel and stops if it ever disagrees with the curve above. Cheap - one
-- extra float read per frame - and it turns "the index drifted in a patch" from a silently wrong
-- number into a message and a correct one.
local curve_trusted = true
local curve_doubts = 0

local function chain_bonus()
    local chain = chain_count:get()
    if chain == nil then
        return nil, nil
    end

    local expected = chain_bonus_from_table(chain)
    if not curve_trusted then
        return expected, chain
    end

    local live = chain_curve:get()
    if live == nil then
        return expected, chain
    end

    -- The curve interpolates its keys with a spline, this table with straight lines, so they differ
    -- slightly between keys. The tolerance is for that; an index pointing at an unrelated channel
    -- misses by far more.
    if math.abs(live - expected) > 0.35 then
        curve_doubts = curve_doubts + 1
        if curve_doubts > 30 then
            curve_trusted = false
            tw.warn("chain curve channel disagrees with the known keys - falling back to ChainCount")
        end
        return expected, chain
    end

    curve_doubts = 0
    return live, chain
end

-- How long until the pass that decides whether the chain survives, as the text to print and the
-- fraction of the window still to run (DESIGN.md §5).
--
-- Read from the game every frame rather than counted down here. The game's timer stops while blocks
-- are sliding and can be pushed backwards by Do_ResetMatchingTimer; a clock of our own would do
-- neither, and would promise time the player has not got.
local function drop_timer()
    local window = match_ticks:get()
    local elapsed = match_timer:get()
    if window == nil or elapsed == nil or window <= 0 then
        return nil, 0
    end

    local left = window - elapsed
    if left < 0 then
        -- The pass fires on the frame the window is exceeded, so this is only ever a frame wide.
        left = 0
    elseif left > window then
        -- Do_ResetMatchingTimer subtracts a whole window rather than clamping, which leaves the
        -- counter negative while match collection is suppressed - during a tutorial, or under a
        -- Hintman freeze.
        left = window
    end

    return string.format("%.2f", left / TICKS_PER_SECOND), left / window
end

-- Bonus 0 (within float noise) is white; above that the colour walks the block palette, one stop
-- per envelope key. Using the key ordinal as the gradient axis rather than the bonus itself is the
-- whole point: the bonus steps are 0.5/1.0/0.5/0.5/0.5, so a gradient keyed on the bonus would
-- crawl through one band and sprint through the rest. On the ordinal axis the colour advances once
-- per tier, and the game's own curve keeps the tiers where the game put them.
local function chain_colour(bonus)
    if bonus < 1e-4 then
        return WHITE
    end

    for i = 1, #CHAIN_KEYS - 1 do
        local y0 = CHAIN_KEYS[i][2]
        local y1 = CHAIN_KEYS[i + 1][2]
        if bonus <= y1 then
            local from = (i == 1) and WHITE or COLOURS[i - 1].colour
            local to = COLOURS[i].colour
            local span = y1 - y0
            return mix(from, to, span > 0 and (bonus - y0) / span or 1)
        end
    end

    return COLOURS[#CHAIN_KEYS - 1].colour
end

return {
    MAX = CHAIN_MAX,
    FIRST = CHAIN_FIRST,
    bonus = chain_bonus,
    drop_timer = drop_timer,
    colour = chain_colour,
}
