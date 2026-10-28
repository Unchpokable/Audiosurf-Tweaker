-- ---------------------------------------------------------------------------------------------
-- The minimap's colours: what each stretch of the track is mostly made of (DESIGN.md §11)
-- ---------------------------------------------------------------------------------------------
--
-- The track is cut into stops of `batch` rings. A stop's colour is its blocks' colours, each weighted by
-- how many blocks there are times what a block of that colour is worth, with the weights raised to a
-- power first - sharpened - so the colour that dominates the stretch takes the stop. The line between
-- stops is a gradient, so the map changes colour where the track does, smoothly.
--
-- Both of those are there because the first version got the colours visibly wrong. Counted block for
-- block, purple wins almost everywhere: it is the commonest block on any track, the quiet one. And mixed
-- with a soft power over a wide window, a stop became an average of four colours, which in RGB is a
-- colour nobody put on the track - yellow and red with a little blue in them come out light green. So
-- the default weights are the game's own block values (StatCollector ChannelSwitch #128: 1, 2, 3, 5, 8,
-- purple to red - a red block is worth eight purple ones), the power is steep, and the neighbours that
-- are blended in are at most two either side. The weights are the player's to change.
--
-- White and wild blocks colour nothing. A stop with no coloured blocks takes the colour of its nearest
-- neighbours instead of going grey; a track with one colour only - Mono, Ninja, Freeride, songs tagged
-- to play that way - is that colour from end to end, which is the colour the table draws it in.
--
-- The work is split by how often its inputs change. Weights per vertex of the line: when the track is
-- scanned or a setting moves. Colours: when the palette or the theme does, which this checks every call -
-- five numbers compared - and which is a weighted sum per vertex when it happens.

local colours = require("colours")
local scan = require("scan")
local track = require("track")

local COLOUR_BY_ID = colours.COLOUR_BY_ID
local totals = scan.totals
local FINE, COLOURED = scan.FINE, scan.COLOURED

-- `batch` is in rings and a multiple of FINE (scan.lua keeps counts per FINE rings); ~96 rings is about
-- two seconds of song. `softness` 0..1: 0 the leader taking all and no blending with the neighbours, 1
-- a broader blend. `value` is what one block of each colour, purple to red, weighs. `revision` moves
-- with every change, so the weights know to rebuild.
local DEFAULT_VALUE = { 1, 2, 3, 5, 8 }
local settings = { batch = 96, softness = 0.5, value = { 1, 2, 3, 5, 8 }, revision = 0 }

-- Normalised weight of bucket b at vertex i: weights[(i - 1) * COLOURED + b + 1]. `covered[i]` false
-- when no stop anywhere near vertex i has a coloured block.
local weights = {}
local covered = {}
-- The one bucket the track carries, when it carries only one.
local solid = nil
local any_colour = false

local built = { scan = -1, track = -1, settings = -1 }

-- The finished colours, one per vertex, and what they were made from.
local out = {}
local made_from = { -1, -1, -1, -1, -1, -1 }
local fresh = false

local function rebuild_weights()
    local rings = track.rings
    local fine = totals.fine
    local batch = settings.batch
    local per = math.max(1, math.floor(batch / FINE + 0.5))
    batch = per * FINE
    local stops = math.max(1, math.ceil(rings / batch))

    -- Weighted counts per stop, and plain counts per bucket over the whole track.
    local value = settings.value
    local raw = {}
    local whole = { 0, 0, 0, 0, 0 }
    for j = 0, stops - 1 do
        for f = j * per, (j + 1) * per - 1 do
            for b = 0, COLOURED - 1 do
                local v = fine[f * COLOURED + b + 1]
                if v then
                    local k = j * COLOURED + b + 1
                    raw[k] = (raw[k] or 0) + v * value[b + 1]
                    whole[b + 1] = whole[b + 1] + v
                end
            end
        end
    end

    solid = nil
    local carried = 0
    for b = 0, COLOURED - 1 do
        if whole[b + 1] > 0 then
            carried = carried + 1
            solid = b
        end
    end
    any_colour = carried > 0
    if carried ~= 1 then
        solid = nil
    end
    if solid ~= nil or not any_colour then
        return
    end

    -- Smoothed over 2R+1 stops with a triangular kernel, then sharpened and normalised. A stop with
    -- nothing in reach is left out (has[j] nil) and filled from its neighbours below.
    local softness = settings.softness
    local reach = math.floor(softness * 2 + 0.5)
    local power = 8 - 6 * softness
    local stop_w = {}
    local has = {}
    local acc = { 0, 0, 0, 0, 0 }
    for j = 0, stops - 1 do
        for b = 1, COLOURED do
            acc[b] = 0
        end
        for d = -reach, reach do
            local s = j + d
            if s >= 0 and s < stops then
                local k = reach + 1 - math.abs(d)
                for b = 0, COLOURED - 1 do
                    local v = raw[s * COLOURED + b + 1]
                    if v then
                        acc[b + 1] = acc[b + 1] + k * v
                    end
                end
            end
        end
        local sum = 0
        for b = 1, COLOURED do
            if acc[b] > 0 then
                acc[b] = acc[b] ^ power
                sum = sum + acc[b]
            end
        end
        if sum > 0 then
            has[j] = true
            for b = 1, COLOURED do
                stop_w[j * COLOURED + b] = acc[b] / sum
            end
        end
    end

    -- The nearest stop with colour at or before / at or after each stop.
    local before, after = {}, {}
    local last = nil
    for j = 0, stops - 1 do
        if has[j] then
            last = j
        end
        before[j] = last
    end
    last = nil
    for j = stops - 1, 0, -1 do
        if has[j] then
            last = j
        end
        after[j] = last
    end

    -- Each vertex sits between the two stops whose centres are either side of its ring; its weights are
    -- theirs, mixed by how close it is to each. Mixing the weights and then the colours is the same as
    -- mixing the colours, because both steps are linear.
    for i = 1, track.points do
        local u = track.ring_of(i, rings) / batch - 0.5
        local j0 = math.min(math.max(math.floor(u), 0), stops - 1)
        local j1 = math.min(j0 + 1, stops - 1)
        local a = before[j0] or after[j0]
        local b = after[j1] or before[j1]
        local base = (i - 1) * COLOURED
        if a == nil then
            covered[i] = false
        else
            covered[i] = true
            local t = 0
            if b ~= nil and b ~= a then
                t = math.min(math.max((u - a) / (b - a), 0), 1)
            else
                b = a
            end
            for k = 1, COLOURED do
                local wa = stop_w[a * COLOURED + k]
                local wb = stop_w[b * COLOURED + k]
                weights[base + k] = wa + (wb - wa) * t
            end
        end
    end
end

local function channels(c)
    return c % 256, math.floor(c / 256) % 256, math.floor(c / 65536) % 256
end

local function rebuild_colours(neutral)
    local n = track.points
    if solid ~= nil or not any_colour then
        local c = solid ~= nil and COLOUR_BY_ID[solid].colour or neutral
        for i = 1, n do
            out[i] = c
        end
        return
    end

    local cr, cg, cb = {}, {}, {}
    for b = 0, COLOURED - 1 do
        cr[b + 1], cg[b + 1], cb[b + 1] = channels(COLOUR_BY_ID[b].colour)
    end
    for i = 1, n do
        if covered[i] then
            local base = (i - 1) * COLOURED
            local r, g, bl = 0, 0, 0
            for k = 1, COLOURED do
                local w = weights[base + k]
                r = r + w * cr[k]
                g = g + w * cg[k]
                bl = bl + w * cb[k]
            end
            out[i] = math.floor(r + 0.5) + math.floor(g + 0.5) * 256 + math.floor(bl + 0.5) * 65536 + 0xFF000000
        else
            out[i] = neutral
        end
    end
end

-- The colour of every vertex of the minimap's line, raw (not faded), as one table that is refilled in
-- place - or nil until both the track's profile and its blocks have been walked. `neutral` is what a
-- vertex with no coloured block anywhere near it gets.
local function update(neutral)
    if not track.ready or not totals.ready or track.rings < 2 then
        return nil
    end

    if built.scan ~= totals.revision or built.track ~= track.revision or built.settings ~= settings.revision then
        rebuild_weights()
        built.scan, built.track, built.settings = totals.revision, track.revision, settings.revision
        fresh = false
    end

    local changed = not fresh or made_from[6] ~= neutral
    for b = 0, COLOURED - 1 do
        if made_from[b + 1] ~= COLOUR_BY_ID[b].colour then
            changed = true
        end
    end
    if changed then
        rebuild_colours(neutral)
        for b = 0, COLOURED - 1 do
            made_from[b + 1] = COLOUR_BY_ID[b].colour
        end
        made_from[6] = neutral
        fresh = true
    end

    return out
end

-- For the settings tab: rings per colour stop (rounded to a multiple of 16), softness 0..1, and the
-- weight of one block of each colour, purple to red (nil keeps the current ones).
local function configure(batch, softness, value)
    settings.batch = math.max(FINE, math.floor(batch / FINE + 0.5) * FINE)
    settings.softness = math.min(math.max(softness, 0), 1)
    if value ~= nil then
        for b = 1, COLOURED do
            settings.value[b] = math.max(0, value[b] or DEFAULT_VALUE[b])
        end
    end
    settings.revision = settings.revision + 1
end

return {
    update = update,
    configure = configure,
    settings = settings,
    DEFAULT_VALUE = DEFAULT_VALUE,
}
