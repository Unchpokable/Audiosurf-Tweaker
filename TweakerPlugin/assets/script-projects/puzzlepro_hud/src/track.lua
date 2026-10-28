-- ---------------------------------------------------------------------------------------------
-- The track's profile, for the minimap (DESIGN.md §11)
-- ---------------------------------------------------------------------------------------------
--
-- The game's minimap is the track seen from the side: the height of every ring of Highway's spine against
-- its number. This samples the same thing at POINTS rings spread evenly along the track, which is all a
-- line a few hundred pixels long can show - the game itself bakes it into 512 columns. The track is
-- several thousand rings long, and its height is a running sum of slopes, so it is smooth enough that
-- every ring would add nothing but cost.
--
-- Spread over frames like scan.lua's walk, for the same reason, and run on the same events: the rings
-- are already there at the first of them (Do_ResetStats), because the rows of the track refer to them.

local ch = require("channels")

local num_rings, lowest_ring, highest_ring = ch.num_rings, ch.lowest_ring, ch.highest_ring
local ring_pos, ring_time = ch.ring_pos, ch.ring_time

local POINTS = 256
local PER_FRAME = 128

-- `height[i]` is 0 at the lowest ring of the track and 1 at the highest; `time[i]` is seconds into the
-- song. Point i is ring ring_of(i). `revision` moves with every finished walk.
local track = {
    ready = false,
    running = false,
    revision = 0,
    points = POINTS,
    rings = 0,
    height = {},
    time = {},
}

local walk = { at = 0, rings = 0, low = 0, span = 0, height = {}, time = {} }

local function ring_of(i, rings)
    return math.floor((i - 1) * (rings - 1) / (POINTS - 1))
end

local function begin()
    track.running = true
    walk.at = 0
    walk.rings = 0
    walk.height = {}
    walk.time = {}
end

local function step()
    if not track.running then
        return
    end

    -- Asked before the track exists, or before Highway has loaded: try again next frame.
    if walk.rings == 0 then
        local rings = num_rings:get()
        local low, high = lowest_ring:get(), highest_ring:get()
        if rings == nil or rings < 2 or low == nil or high == nil or high <= low then
            return
        end
        walk.rings = math.floor(rings)
        walk.low = low
        walk.span = high - low
    end

    local budget = PER_FRAME
    while walk.at < POINTS and budget > 0 do
        local i = walk.at + 1
        local ring = ring_of(i, walk.rings)
        local _, y = ring_pos:get(ring)
        if y == nil then
            return
        end
        walk.height[i] = math.min(math.max((y - walk.low) / walk.span, 0), 1)
        walk.time[i] = ring_time:get(ring) or 0
        walk.at = i
        budget = budget - 1
    end

    if walk.at >= POINTS then
        track.running = false
        track.rings = walk.rings
        track.height = walk.height
        track.time = walk.time
        track.ready = true
        track.revision = track.revision + 1
    end
end

-- Where a ring (fractional) is in the sample, as a fractional point index from 1 to POINTS.
local function point_at(ring)
    local rings = track.rings
    if rings < 2 then
        return 1
    end
    local p = 1 + ring * (POINTS - 1) / (rings - 1)
    return math.min(math.max(p, 1), POINTS)
end

-- A sampled series read at a fractional point index, linearly between the two samples either side.
local function sample(series, p)
    local i = math.floor(p)
    if i >= POINTS then
        return series[POINTS]
    end
    local t = p - i
    return series[i] + (series[i + 1] - series[i]) * t
end

-- Seconds of song left from a ring to the end. Interpolated from the samples rather than read off the
-- ring: a sample is ~50 rings apart, about a second, and time along the track is close to linear across
-- one - the error is far under the second this is printed to. One array read a frame avoided.
local function time_left(ring)
    if not track.ready then
        return nil
    end
    return math.max(0, track.time[POINTS] - sample(track.time, point_at(ring)))
end

local function run_started()
    begin()
end

tw.on_call("StatCollector", "Do_ResetStats", "after", run_started)
tw.on_call("StatCollector", "Do_ReseteWhiteWildBlocks", "after", run_started)
tw.on_call("StatCollector", "Do_ResetSimpleStats", "after", run_started)

track.begin = begin
track.step = step
track.ring_of = ring_of
track.point_at = point_at
track.sample = sample
track.time_left = time_left

return track
