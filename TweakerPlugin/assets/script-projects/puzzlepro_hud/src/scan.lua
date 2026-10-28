-- ---------------------------------------------------------------------------------------------
-- How much traffic of each colour the track carries: walk the live pattern (DESIGN.md §2).
--
-- Spread over frames on purpose. Each row costs a cursor write, a vector read and a cursor restore,
-- and a track can be several thousand rows - doing it in one frame would be a visible hitch at the
-- exact moment the player is starting a run.
--
-- The previous total stays on screen while a rescan runs. It is the previous run's answer, but the
-- alternative is blanking the table for a quarter second at every start, and on a restart the two
-- are usually the same number anyway.
-- ---------------------------------------------------------------------------------------------

local colours = require("colours")
local ch = require("channels")

local COLOURS, BUCKET = colours.COLOURS, colours.BUCKET
local car_count, traffic_pattern = ch.car_count, ch.traffic_pattern

local ROWS_PER_FRAME = 128

-- The minimap's colour comes from where on the track each block is (DESIGN.md §11), so the walk also files
-- every coloured block under the stretch of track it is on: FINE rings per stretch, ~a third of a second.
-- That is finer than any colour stop the minimap draws - its setting is a multiple of this - so changing
-- the setting re-groups these counts instead of walking the track again.
--
-- Flat: stretch s, bucket b (0..4, the five block colours) is fine[s * COLOURED + b + 1], nil meaning 0.
-- White and wild (bucket 7) are not kept: they colour nothing on the map.
local FINE = 16
local COLOURED = 5

-- Which colour columns the table shows. Derived from the track rather than from a table of modes,
-- because the game recolours the track per character and league before the run and the pattern is
-- the result - StatCollector::Do_CharacterAndLeagueTrafficMods (#904), called from Do_ResetStats:
--
--     LeagueID == 1        Do_LimitTo4ColorsForProMode (#886)  green(2) -> blue(1)
--     ThinTraffic?/Ninja?  Do_TurnPurplesBlue (#877)           purple(0) -> blue(1)
--     Ninja?/Freeride?     Do_NinjaMagic (#403) rewrites EVERY row:
--                              x := (x > NinjaColorCutoff) ? 3 : Puzzle::StoneColorID
--     isMechMode           yellow -> red, blue -> green
--     PortalMode?          purple -> blue, yellow -> green, red -> green
--
-- So Casual runs three colours, Pro four, Mono exactly two - the yellow slot and the stone slot -
-- and a column for a colour the track does not carry is a column of dashes. Reading it off the
-- pattern gets all six cases and every future one for free; a mode table would get five of them and
-- then go quietly stale.
local function visible_columns(counts)
    local out = {}
    for _, c in ipairs(COLOURS) do
        if (counts[c.id] or 0) > 0 then
            out[#out + 1] = c
        end
    end
    -- No colours at all is a scan that went wrong, not a track without colours. Showing everything
    -- is the honest way to say "no idea" - collapsing to nothing would hide the fault.
    if #out == 0 then
        return COLOURS
    end
    return out
end

-- `revision` moves every time a walk finishes, so whatever is built from the result (the minimap's colours)
-- can tell a new track from the one it already has.
local totals = {
    ready = false,
    by_row = {},
    running = false,
    at = 0,
    rows = 0,
    partial = {},
    columns = COLOURS,
    fine = {},
    fine_partial = {},
    revision = 0,
}

local function begin_scan()
    totals.running = true
    totals.at = 0
    totals.rows = math.floor((car_count:get() or 0) + 0.5)
    totals.partial = {}
    for _, c in ipairs(COLOURS) do
        totals.partial[c.id] = 0
    end
    totals.fine_partial = {}
end

local function step_scan()
    if not totals.running then
        return
    end

    if totals.rows <= 0 then
        -- Asked before the track existed. Try again next frame rather than publishing zero.
        totals.rows = math.floor((car_count:get() or 0) + 0.5)
        if totals.rows <= 0 then
            return
        end
    end

    local budget = ROWS_PER_FRAME
    while totals.at < totals.rows and budget > 0 do
        local id, ring, lane = traffic_pattern:get(totals.at)
        -- Not resolvable yet (group still loading): stop, keep the position, try next frame.
        if id == nil then
            return
        end

        -- An all-zero row is not a block. Traffic thinning takes a car off the track by blanking its
        -- whole row - Do_NeuterThisCar (#680) writes Value Vector#684, which is (0, 0, 0) - and it
        -- runs for every character with SpecialPurpose::ThinTraffic? as well as for every Ninja:
        --
        --     Ninja?    : x > 5 && (y - LastAllowedRing) < RequiredSeparation  -> neuter
        --     otherwise : x < 3 && (same separation test)                      -> neuter
        --
        -- The row stays in the array, so a walk that only looks at x counts it as colour id 0 and
        -- reports a purple column full of blocks nobody can collect. The non-Ninja branch hides
        -- this by accident: Do_TurnPurplesBlue (#877) runs on the same row a moment later and turns
        -- the zero into a blue. The Ninja branch has no such step, which is why Mono was the mode it
        -- showed up in.
        --
        -- Testing all three components rather than just the colour is the point: a genuine purple
        -- block has a ring, and the game's own count (Do_GetTrafficCounts) makes exactly this
        -- mistake - its switch files id 0 under PurpleTotal without looking any further. Dropping
        -- these rows cannot move the bonus prediction, because neutering never touches yellow or
        -- red in either branch.
        if id ~= 0 or (ring or 0) ~= 0 or (lane or 0) ~= 0 then
            -- floor, not round: Do_PlaceWilds writes 6.1, and the engine's own switch truncates.
            local bucket = BUCKET[math.floor(id)]
            if bucket then
                totals.partial[bucket] = totals.partial[bucket] + 1
                if bucket < COLOURED then
                    local slot = math.floor((ring or 0) / FINE) * COLOURED + bucket + 1
                    local fine = totals.fine_partial
                    fine[slot] = (fine[slot] or 0) + 1
                end
            end
        end

        totals.at = totals.at + 1
        budget = budget - 1
    end

    if totals.at >= totals.rows then
        totals.running = false
        totals.by_row = totals.partial
        totals.fine = totals.fine_partial
        totals.revision = totals.revision + 1
        -- Settled once per scan, not per frame: the column set is layout, and layout that is
        -- recomputed while the player is looking at it is layout that moves.
        totals.columns = visible_columns(totals.partial)
        totals.ready = true
    end
end

return {
    totals = totals,
    begin = begin_scan,
    step = step_scan,
    FINE = FINE,
    COLOURED = COLOURED,
}
