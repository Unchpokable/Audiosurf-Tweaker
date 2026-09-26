-- ---------------------------------------------------------------------------------------------
-- The score feed: a queue of points in flight (DESIGN.md §8, §9)
--
-- One entry per COLOUR per frame, laid out as a queue to the left of the score. Each entry lives
-- until the last of the particles behind it has landed, which is when the game has finished adding
-- its points.
-- ---------------------------------------------------------------------------------------------

local ch = require("channels")

local points, update_colour = ch.points, ch.update_colour
local launch_money, launch_colour, particle_at = ch.launch_money, ch.launch_colour, ch.particle_at

-- Six colours is the most one collection can produce, and the queue drains as the particles land.
-- The cap is a guard against a pathological frame, not a design parameter.
local FLOATER_LIMIT = 12
-- Each entry in a queue appears this much later than the one in front of it, so a multicolour
-- collection reads as a run of numbers rather than as one event with several labels.
local FLOATER_STAGGER = 0.05
local FLOATER_FADE_IN = 0.12
-- How long an arrived number takes to dissolve into the score.
local FLOATER_GONE_SECONDS = 0.22
-- For entries with no particle behind them - the ram bonus, the penalties. Their points are already
-- in the total, so the number is trailing the score rather than leading it, and should not linger.
local FLOATER_LOOSE_SECONDS = 0.45
-- A particle that never reports landing would pin its number on screen forever. It should not
-- happen - every launch ends in Do_EndLifeApplyScore - but "should not" is not a lifetime policy.
local FLOATER_MAX_SECONDS = 6.0

local floaters = {}
-- Particle landings seen but not yet matched to their Do_UpdatePoints call. A counter rather than a
-- flag because a frame can land several at once.
local announced = 0
local last_points = nil

-- What the headline multiplies raw points by, published once a frame by the draw handler. A floater
-- is frozen against it at the moment it is announced, so the number flying in is the amount the
-- total actually moves (DESIGN.md §10). It lives here rather than being computed on the spot because a
-- launch happens once per collected TILE, and earned_bonus() is half a dozen channel reads.
local live_multiplier = 1

local function reset_score_feed()
    floaters = {}
    announced = 0
    -- Not zero: nil means "no baseline yet", so the first event after a reset cannot be reported as
    -- a delta against the previous run's total.
    last_points = nil
end

-- Take a particle row away from whoever is waiting on it. A launch reuses rows in a ring, so an
-- entry still holding a recycled row would wait for a landing that is now somebody else's.
local function release_row(row)
    for _, f in ipairs(floaters) do
        if f.rows[row] then
            f.rows[row] = nil
            f.pending = f.pending - 1
            return
        end
    end
end

local function push_floater(amount, colour_id, row)
    if amount == nil then
        return
    end
    -- Rounded the way the game rounds it for display. Sub-unit deltas exist - the scalers are
    -- fractional - and a floater reading "+0" is noise.
    amount = (amount >= 0) and math.floor(amount + 0.5) or -math.floor(-amount + 0.5)
    if amount == 0 then
        return
    end

    local id = colour_id and math.floor(colour_id + 0.5) or nil
    if row ~= nil then
        release_row(row)
    end

    -- Coalesce by colour within the frame. The game launches a particle per matched TILE - a
    -- seven-block match is seven launches carrying the same colour and the same PointsThisTile - so
    -- without this a single match puts seven identical numbers on top of each other. The game's own
    -- text floaters merge the same way, one per colour, because they are built by diffing
    -- PointsByColor once a frame.
    for _, f in ipairs(floaters) do
        if f.frame == tw.frame and f.colour_id == id then
            f.amount = f.amount + amount
            if row ~= nil then
                f.rows[row] = true
                f.pending = f.pending + 1
                f.tracked = true
            end
            return
        end
    end

    if #floaters >= FLOATER_LIMIT then
        table.remove(floaters, 1)
    end

    -- Where this entry sits in its own frame's queue, which is also how long it waits before
    -- appearing.
    local ahead = 0
    for _, f in ipairs(floaters) do
        if f.frame == tw.frame then
            ahead = ahead + 1
        end
    end

    local entry = {
        amount = amount,
        -- Frozen, not read at draw time: a number that changed while it was on screen would be
        -- worse than one that is a few percent behind a bonus earned mid-flight.
        mult = live_multiplier,
        colour_id = id,
        frame = tw.frame,
        rows = {},
        pending = 0,
        tracked = false,
        age = 0,
        delay = ahead * FLOATER_STAGGER,
        gone = 0,
        -- Where it is drawn, eased towards its slot. nil until the first frame that places it, so a
        -- new entry starts in its slot rather than sliding in from wherever the list happened to be.
        x = nil,
    }

    if row ~= nil then
        entry.rows[row] = true
        entry.pending = 1
        entry.tracked = true
    end

    floaters[#floaters + 1] = entry
end

-- Advance every entry. Called before the visibility gate, so a floater that was launched just as
-- the player paused does not sit frozen waiting to replay itself later.
local function step_floaters()
    -- Re-seed the baseline the delta hook works from. Doing it here rather than at the reset means
    -- it is taken once the game has actually zeroed Points, so the first scoring event of a run that
    -- did not come through a particle still gets a floater instead of being spent on the baseline.
    if last_points == nil then
        last_points = points:get()
    end

    if #floaters == 0 then
        return
    end

    local dt = tw.dt()
    local i = 1
    while i <= #floaters do
        local f = floaters[i]
        f.age = f.age + dt

        -- An entry with particles behind it leaves when the last of them has landed - that is the
        -- frame the game finished adding its points. One without leaves on a short clock of its own.
        local done
        if f.tracked then
            done = f.pending <= 0
        else
            done = (f.age - f.delay) >= FLOATER_LOOSE_SECONDS
        end

        if f.age > FLOATER_MAX_SECONDS then
            done = true
        end

        if done then
            f.gone = f.gone + dt / FLOATER_GONE_SECONDS
        end

        if f.gone >= 1 then
            table.remove(floaters, i)
        else
            i = i + 1
        end
    end
end

-- A match was scored: the particle has been created but the points have not been awarded yet. This
-- is the moment the number should appear, and Index_Particles still points at the row that was just
-- filled, which is what ties this entry to that particular particle.
tw.on_call("MoneyFloaterCommander", 27, "after", function()
    local row = particle_at:get()
    push_floater(launch_money:get(), launch_colour:get(), row and math.floor(row + 0.5) or nil)
end)

-- A particle reached the score box. Runs immediately before the Do_UpdatePoints call it makes, so
-- the counter is always armed by the time the hook below sees the change - and Index_Particles is
-- still on the row that landed, which is how the right queue entry is credited.
tw.on_call("MoneyFloaterCommander", 198, "before", function()
    announced = announced + 1

    local row = particle_at:get()
    if row ~= nil then
        release_row(math.floor(row + 0.5))
    end
end)

-- Everything else that moves the score: ninja ram bonuses, penalties, question boxes, boss damage.
-- The delta is measured rather than read (DESIGN.md §8).
tw.on_call("StatCollector", 153, "after", function()
    local now = points:get()
    if now == nil then
        return
    end

    local previous = last_points
    last_points = now

    if announced > 0 then
        announced = announced - 1
        return
    end

    if previous == nil then
        return
    end

    -- A run reset between two events: Points goes to zero without anyone calling Do_UpdatePoints,
    -- and reporting that as a delta would throw a five-figure penalty on screen.
    if now == 0 and previous > 0 then
        return
    end

    push_floater(now - previous, update_colour:get(), nil)
end)

-- The same two events the tally resets on, for the same reason: a pause-menu restart fires
-- Do_ResetSimpleStats and not Do_ResetStats, and a feed that only watched the latter would carry the
-- previous run's baseline - and its in-flight numbers - into the new one.
tw.on_call("StatCollector", "Do_ResetStats", "after", reset_score_feed)
tw.on_call("StatCollector", "Do_ResetSimpleStats", "after", reset_score_feed)

return {
    FADE_IN = FLOATER_FADE_IN,
    step = step_floaters,
    -- A function rather than the table: a reset replaces the queue with a new one, and a reader
    -- holding the old table would draw a queue nobody updates any more.
    floaters = function()
        return floaters
    end,
    -- Published once a frame by the draw handler, which is the one place the multiplier is computed.
    set_multiplier = function(value)
        live_multiplier = value
    end,
}
