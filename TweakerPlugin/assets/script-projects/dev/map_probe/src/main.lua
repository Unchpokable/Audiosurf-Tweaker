-- WHAT THIS IS FOR
--
-- Ф5 of Docs/Internal/lua-engine-fix-roadmap.md: a script built by twllink from several files is one
-- file to the game, and a runtime error in it names a line of that one file - unless the plugin reads
-- the .lua.map twllink wrote beside it and turns the position back into a module and a line of it.
--
-- This project is built with comments stripped (twproject.json), so the bundle has fewer lines than
-- its sources and a map that only knew where each module starts would be wrong inside every module.
--
-- Copy BOTH files flat into engine\TweakerStuff\Scripts\ - map_probe.lua and map_probe.lua.map,
-- from TweakerPlugin\assets\scripts\dev\ (dev\ is not scanned and not shipped). Then open the
-- Scripts tab and look at this script's row. What should be there is in src/faulty/thrower.lua.

local thrower = require("faulty.thrower")

tw.on_frame(function()
    thrower.tick()
end)
