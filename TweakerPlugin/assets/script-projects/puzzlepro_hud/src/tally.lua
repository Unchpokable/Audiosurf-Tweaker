-- ---------------------------------------------------------------------------------------------
-- Run tally, counted by track row
-- ---------------------------------------------------------------------------------------------

local colours = require("colours")
local ch = require("channels")
local scan = require("scan")

local COLOURS, BUCKET = colours.COLOURS, colours.BUCKET
local type_ch, traffic_id, current_traffic, precalc_row = ch.type_ch, ch.traffic_id, ch.current_traffic, ch.precalc_row
local scooping, erasing = ch.scooping, ch.erasing
local begin_scan = scan.begin

local taken = {}
local by_route = { grid = 0, buffer = 0, erased = 0 }
-- Row ids already spent. A ghost (DESIGN.md §1) arrives carrying an id we have seen, which is exactly
-- what makes it identifiable.
local seen = {}
local powerups, ghosts = 0, 0

local function reset_tally()
    for _, c in ipairs(COLOURS) do
        taken[c.id] = 0
    end
    by_route.grid, by_route.buffer, by_route.erased = 0, 0, 0
    seen = {}
    powerups, ghosts = 0, 0
end

reset_tally()

-- Counts one collected block against the track row it came from. Returns whether it counted.
--
-- An id we have already spent means a ghost: a car the game birthed into a freed slot without
-- advancing the pattern cursor, so it is a duplicate of a row that is still going to spawn on its
-- own. It has no row in the denominator and must not be counted into the numerator - but it is a
-- real block the player collected, so it is reported rather than discarded silently.
local function count_row(id, colour)
    local bucket = BUCKET[math.floor(colour + 0.5)]
    if bucket == nil then
        return false
    end

    -- No usable row id - the channel has not resolved yet, or the game left -1 there. Count the
    -- block rather than drop it: that is the old, over-counting behaviour, and losing accuracy is
    -- the right way for a missing discriminator to degrade. Losing blocks is not.
    if id ~= nil and id >= 0 then
        if seen[id] then
            ghosts = ghosts + 1
            return false
        end
        seen[id] = true
    end

    taken[bucket] = (taken[bucket] or 0) + 1
    return true
end
-- Three events, and all three are needed. They are registered at the bottom of this file, where the
-- scan they arm (scan.lua) is at hand.
-- bottom of this section.
--
--   Do_ResetStats (#1)          the track is being regenerated. Called only from
--                               Do_GenerateSongStats, i.e. when a new song loads.
--
--   Do_ReseteWhiteWildBlocks    the whites, wilds and power-ups are being (re)placed over the
--   (#1156, the typo is theirs)  generated colours. Called from XX_StartHere::Do_ReturnToStart,
--                               inside its OneTime, so once per entry into a run. **This is the
--                               event that says the track is finally what the player will see**,
--                               and missing it is what made the denominators wrong: white
--                               placement uses plain RAND, so it lands somewhere different on
--                               every single run of the same song.
--
--   Do_ResetSimpleStats (#345)  the run counters are being zeroed. Called from Do_ReturnToStart
--                               too, after the OneTime above, so it is also the safe backstop for
--                               "the pattern is settled". Restarting from the pause menu fires
--                               this and NOT Do_ResetStats - the track is not regenerated - and a
--                               tracker hooked only on the latter keeps counting into the previous
--                               run's tally and sails past 100%.

-- The hooks described above. A hook runs on the game's own call stack, so all it does is arm the
-- scan; the walking happens in on_frame.
local function run_started()
    reset_tally()
    begin_scan()
end

tw.on_call("StatCollector", "Do_ResetStats", "after", run_started)
tw.on_call("StatCollector", "Do_ReseteWhiteWildBlocks", "after", run_started)
tw.on_call("StatCollector", "Do_ResetSimpleStats", "after", run_started)

-- ---------------------------------------------------------------------------------------------
-- Collection events
-- ---------------------------------------------------------------------------------------------

-- "after": TrafficType and TrafficID are the first two things the handler writes, so both are valid
-- by the time this runs, and so are the two ability flags that say which branch was taken.
tw.on_call("TrafficCommander", "Do_CollectCar", "after", function()
    local colour = type_ch:get()
    if colour == nil then
        return
    end

    if not count_row(traffic_id:get(), colour) then
        return
    end

    local scoop = scooping:get()
    local erase = erasing:get()
    if scoop and scoop ~= 0 then
        by_route.buffer = by_route.buffer + 1
    elseif erase and erase ~= 0 then
        by_route.erased = by_route.erased + 1
    else
        by_route.grid = by_route.grid + 1
    end
end)

-- The other half of the funnel. A power-up block never reaches Do_CollectCar, so without this the
-- tally is short one block of that colour for every power-up taken.
--
-- Both reads are still valid in the "after" phase: TrafficType is written before the rebirth, and
-- the rebirth writes Traffic.Type[slot] rather than the TrafficType channel. CurrentTraffic was put
-- back on the power-up's slot by Set Value #1201, and the inner Do_BirthCar does not touch
-- PrecalcedArrayNum - that lives in the wrapper it bypassed.
tw.on_call("TrafficCommander", "Do_CollectQuestionBox", "after", function()
    local colour = type_ch:get()
    if colour == nil then
        return
    end

    local slot = current_traffic:get()
    local id = slot and precalc_row:get(slot) or nil

    powerups = powerups + 1
    count_row(id, colour)
end)

return {
    taken = taken,
    by_route = by_route,
}
