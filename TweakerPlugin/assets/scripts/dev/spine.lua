-- @name        Frame spine probe
-- @author      Audiosurf Tweaker
-- @version     1.0
-- @description Dev probe for Ф1: shows whether the engine frame spine is live, and how the engine's
--              frame rate compares to the overlay's.
--
-- WHAT THIS ANSWERS
--
-- `EngineControl::EngineLoop` was identified by reading a disassembler, never by running the game
-- (Docs/Internal/reversing-journal-boot.md §1.1). Three things had to be true for the spine to work,
-- and until this ran on a real Audiosurf none of them were more than well-argued:
--
--   1. the object QuestViewer's main loop drives really is an EngineControl;
--   2. the detour goes into a function the game actually calls, once per frame;
--   3. `EngineControl::GetEngineInterface()` hands back the same engine pointer the old CallChannel
--      detour used to catch - and hands it back on the first frame, not after a click in the menu.
--
-- The lifecycle log answers all three ("stage 3: frame spine live (N engine frame(s))"). This draws
-- the same thing on screen, plus the one number the log cannot show: whether the engine's frame and
-- the overlay's frame advance at different rates, which is the entire reason tw.frame moved.
--
-- All three were confirmed on the game on 23.09.2026: ratio 1.00 in the menu, `spine: LIVE`, and the
-- engine pointer arriving with no input at all. See the plan's Ф1 section.
--
-- WHAT IT CANNOT ANSWER, AND WHERE THAT WENT
--
-- The engine's frame rate is not constant: one log showed 130 engine frames in 208 ms - 625 Hz -
-- while the menu runs 1:1 with the overlay. The suspicion is that the graph runs free wherever it is
-- evaluated without being presented (track generation, the loading screen).
--
-- **No script can measure that any more, and this one least of all.** Ф2 holds every script's
-- callbacks until the layer reports `ready`, which is exactly the end of the window in question - so
-- a probe written in Lua is asleep for the whole of it, and during a loading screen there was nobody
-- looking at the overlay anyway.
--
-- So the measurement moved into the layer itself: `engine_state` times the `booting` and `starting`
-- phases in engine frames and in milliseconds and writes them to the lifecycle log on the way into
-- `ready`. Look for `engine: graph rate while loading` in TweakerStuff\Logs\TweakerPlugin.log.
--
-- What this file is still good for is the steady state: engine frames against overlay frames while
-- the game is up, which is the ratio tw.frame exists for.
--
-- Drop this into engine\TweakerStuff\Scripts\ by hand. It lives under dev/ so the bundle never
-- carries it (Docs/Internal/plugin-offline-mode.md, Ф8).

local anchor_x, anchor_y = 24, 24

-- Sampled once a second of wall clock rather than every frame: the interesting number is a rate, and
-- a rate computed over one frame is noise.
local window = { seconds = 0, engine_at = 0, draw_at = 0, engine_rate = 0, draw_rate = 0 }

local last_engine = -1
local ticks_seen = 0

tw.on_tick(function()
    ticks_seen = ticks_seen + 1
end)

tw.on_frame(function()
    local engine = tw.frame
    local draw = tw.draw_frame_count

    window.seconds = window.seconds + tw.dt()
    if window.seconds >= 1.0 then
        window.engine_rate = (engine - window.engine_at) / window.seconds
        window.draw_rate = (draw - window.draw_at) / window.seconds
        window.seconds, window.engine_at, window.draw_at = 0, engine, draw
    end

    -- The spine is live exactly when on_tick has fired at all. tw.frame alone cannot say so: it
    -- falls back to counting draw dispatches when the spine is missing, on purpose, so that scripts
    -- keep resolving their channels either way (src/lua/lua_api.cxx, tw_frame).
    --
    -- Since Ф2 "NOT TICKING" is almost unreachable: on_frame does not run before `ready`, and
    -- `ready` normally means the spine is up. It still catches the one case where it is not - the
    -- spine failed to install, the state machine says so and reports `ready` unconditionally so that
    -- scripts are not silently disabled. That is exactly when this line matters.
    local live = ticks_seen > 0
    local colour = live and tw.theme("text_primary") or tw.theme("text_error")

    local x0, y0 = tw.hud.safe()
    local x, y = x0 + anchor_x, y0 + anchor_y
    local line = tw.hud.font_size() + 4

    tw.hud.text(x, y, live and "spine: LIVE" or "spine: NOT TICKING", colour)
    y = y + line
    tw.hud.text(x, y, string.format("engine %d  (%.1f/s)", engine, window.engine_rate), tw.theme("text_secondary"))
    y = y + line
    tw.hud.text(x, y, string.format("draw   %d  (%.1f/s)", draw, window.draw_rate), tw.theme("text_secondary"))
    y = y + line

    local ratio = window.draw_rate > 0 and (window.engine_rate / window.draw_rate) or 0
    tw.hud.text(x, y, string.format("engine/draw %.2f", ratio), tw.theme("text_muted"))
    y = y + line

    -- Since Ф2 this line is always "ready" or "busy" while anything here is on screen: a script's
    -- on_frame does not run in any other state. It is drawn anyway, because seeing it flip to "busy"
    -- when a run starts loading is the cheapest confirmation that the state machine is alive.
    tw.hud.text(x, y, "state: " .. tw.state(), tw.theme("text_muted"))
    y = y + line
    tw.hud.text(x, y, "engine ready: " .. tostring(tw.engine_ready()), tw.theme("text_muted"))
    y = y + line
    tw.hud.text(x, y, "can write: " .. tostring(tw.can_write()), tw.theme("text_muted"))

    -- Said once, when the spine first ticks, so the notefeed carries the same verdict the log does.
    if live and last_engine < 0 then
        tw.notify("frame spine live at engine frame " .. engine)
    end
    last_engine = engine
end)
