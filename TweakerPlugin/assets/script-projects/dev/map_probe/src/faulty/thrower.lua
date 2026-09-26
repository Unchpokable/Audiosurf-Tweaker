-- Two messages, both of which must show a position in THIS file, not in map_probe.lua:
--
--   warn   faulty/thrower.lua:21   "map probe: said at load"
--   error  faulty/thrower.lua:31   "...faulty/thrower.lua:31: map probe: deliberate error"
--
-- The error is thrown every two seconds of frames, which is far too slow for the error budget (five
-- in 300 frames) to suspend the script: the count next to the message goes up instead. Its tooltip
-- carries the traceback, and every frame of it that is in this bundle must name a module too -
-- faulty/thrower.lua:31 and main.lua:17.
--
-- Wrong would be: map_probe.lua:<n> anywhere (the map was not read), or a position in this file that
-- is off by the number of comment lines above it (the map is only right at module starts).
--
-- Delete map_probe.lua.map, press Reload, and the same messages must name map_probe.lua instead -
-- no map is not an error. Edit map_probe.lua by hand (add a blank line at the top), press Reload,
-- and the row gets one more message: "map_probe.lua.map is ignored: the script was changed after it
-- was built". A map that no longer matches is refused rather than trusted.

local M = {}

tw.warn("map probe: said at load")

local frames = 0

function M.tick()
    frames = frames + 1
    if frames % 120 ~= 0 then
        return
    end
    -- Line 31: the one the error must point at.
    error("map probe: deliberate error")
end

return M
