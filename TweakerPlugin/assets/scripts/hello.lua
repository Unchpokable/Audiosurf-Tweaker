-- @name        Hello
-- @author      Audiosurf Tweaker
-- @version     1.0
-- @description Smallest working example: reads the graph, draws against the overlay's layout
--
-- Audiosurf Tweaker - LuaJIT scripting, first light.
--
-- Copy this file (or the whole scripts/ folder) next to TweakerPlugin.dll. Everything it needs is
-- already in the environment: there is no require(), and there is no ffi - the host built the fast
-- bindings before this file was loaded and then took the library away (see
-- Docs/Internal/lua-scripting.md §6).
--
-- What it proves, in order of how much it took to get there:
--   1. a script loads and runs inside the game;
--   2. it can talk back - notefeed toast, plugin log;
--   3. it can draw its own HUD over the game;
--   4. it can read the game's Quest3D channel graph live.

tw.notify("Lua is alive")
tw.log("hello.lua loaded")

-- One accessor per channel family, and that is deliberate. The engine's vtable slot 17 is GetFloat
-- on a numeric channel and GetString on a text one, so the accessor is what carries the type. Ask
-- through the wrong one and you get a Lua error naming both types - not a silent nil, and not a
-- jump into whatever happened to sit next to the vtable.
--
-- Resolved lazily: the engine pointer the plugin needs is captured by a detour that only fires once
-- the game calls a channel that does not override CallChannel, which in practice can mean "after the
-- player clicks something in a menu". Asking now and getting nothing is normal and expected.
local timer  = tw.float_ch("StatCollector", "Timer")
local points = tw.float_ch("StatCollector", "Points")
local match  = tw.float_ch("StatCollector", "LargestMatch")

-- A text channel, read through the accessor that knows how. Empty until a run finishes - this is the
-- string the results screen builds out of the bonuses earned (see reversing-journal-gameplay.md §3.4).
--
-- Saying tw.float_ch("StatCollector", "Feat String") instead would raise:
--   StatCollector.Feat String is a 'text' channel, not 'float' - use the matching accessor
local feats = tw.string_ch("StatCollector", "Feat String")

local WHITE = 0xFFFFFFFF
local DIM   = 0xFF9A9A9A

local ROW_H = 16

-- Laid out against the overlay's own geometry rather than fixed coordinates, in two steps.
--
-- tw.hud.safe() gives the area below the overlay's top chrome band - the game runs at whatever
-- resolution the player chose, so a hardcoded corner is wrong on somebody's machine.
--
-- tw.hud.widget() then gives one widget's exact rectangle, this frame. Used here to sit directly
-- under the toast column and share its left edge, which safe() alone cannot express: the feed can be
-- on either side of the screen (overlay_config::feed_side), so "under the toasts" is not a fixed
-- corner. Falls back to the safe area's own corner if the feed is not reporting.
--
-- Worth being tidy about, because scripts draw after every overlay widget now - whatever a script
-- puts down goes on top of the overlay's own chrome, so staying out of its way is the script's job.
local function origin()
    local sx0, sy0, sx1, _ = tw.hud.safe()

    local fx0, _, _, fy1 = tw.hud.widget("notefeed")
    if fx0 then
        return fx0, math.max(sy0, fy1) + 16, sx1
    end

    return sx0 + 24, sy0 + 24, sx1
end

local function row(x, y, label, value, unit)
    tw.hud.text(x, y, label, DIM)
    if value == nil then
        tw.hud.text(x + 116, y, "--", DIM)
    else
        tw.hud.text(x + 116, y, string.format("%.1f%s", value, unit or ""), WHITE)
    end
end

tw.on_frame(function()
    local x, y = origin()

    if not tw.engine_ready() then
        tw.hud.text(x, y, "Tweaker Lua: waiting for the engine...", DIM)
        return
    end

    tw.hud.text(x, y, "Tweaker Lua", WHITE)

    -- StatCollector is the group the gameplay journal maps out in full: Timer ticks with the song
    -- and stops when the game pauses, Points is the running score, LargestMatch the biggest match so
    -- far. See Docs/Internal/reversing-journal-gameplay.md §3.
    row(x, y + ROW_H * 1.5, "song time",  timer:get(),  " s")
    row(x, y + ROW_H * 2.5, "points",     points:get())
    row(x, y + ROW_H * 3.5, "best match", match:get())

    local earned = feats:get()
    if earned and earned ~= "" then
        tw.hud.text(x, y + ROW_H * 5, earned, WHITE)
    end
end)
