-- ---------------------------------------------------------------------------------------------
-- Animation
-- ---------------------------------------------------------------------------------------------

-- A named tween. Hand it a key and where the value should be, and it answers where the value is
-- right now - restarting from wherever it had got to if the target moved mid-flight (DESIGN.md §6).
--
-- Keyed by string rather than handing back a handle so that the drawing code stays a straight line.
-- The alternative is a table of tween objects built alongside the layout, which is state to create,
-- find and tear down in three places instead of none.
local ROLL_SECONDS = 0.22

local rolls = {}

local function roll(key, target, seconds, curve)
    if target == nil then
        return nil
    end

    local a = rolls[key]
    if a == nil then
        -- First sight of a value is not a change. Rolling up from zero the moment the widget
        -- appears would animate the whole run's score on the frame it becomes visible.
        rolls[key] = { v = target, from = target, to = target, t = 1 }
        return target
    end

    if math.abs(target - a.to) > 1e-6 then
        a.from, a.to, a.t = a.v, target, 0
    end

    if a.t < 1 then
        a.t = math.min(1, a.t + tw.dt() / (seconds or ROLL_SECONDS))
        a.v = a.from + (a.to - a.from) * tw.ease(curve or "cubicOut", a.t)
    else
        a.v = a.to
    end

    return a.v
end

return roll
