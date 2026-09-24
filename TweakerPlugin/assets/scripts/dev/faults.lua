-- @name        Fault probe
-- @author      Audiosurf Tweaker
-- @version     1.0
-- @description Dev probe for Ф4: fails on purpose, so you can watch it fail alone. Not for players.
--
-- WHAT THIS IS FOR
--
-- Ф4 of Docs/Internal/lua-engine-fix-roadmap.md made the script, not the layer, the unit of failure:
-- each callback runs under its own guard, a script that keeps failing or keeps running long is
-- suspended, and the notefeed gets a line rather than a flood. None of that shows while every script
-- behaves. This one misbehaves - in one way at a time, picked by MODE below - so the rest can be
-- watched carrying on.
--
-- Copy it flat into engine\TweakerStuff\Scripts\ (dev\ is not scanned and not shipped), next to the
-- scripts you want to see survive it - puzzlepro_hud.lua is the obvious one. Edit MODE, press Reload
-- on this script's row, and read the row.
--
-- MODES, AND WHAT EACH SHOULD LOOK LIKE
--
--   "throw" - on_frame throws every frame once a 3 s grace has passed. Expect ONE notefeed line with
--             the error, then within a fraction of a second a second one saying "Fault probe:
--             suspended - 5 errors in ...". The row turns amber and shows Resume, and the probe's
--             own label disappears - it belongs to the same script. Every other script keeps
--             drawing throughout. Resume: it runs again and is suspended again five errors later.
--             Reload: the grace starts over.
--
--   "rare"  - throws once every 10 s. Expect one notefeed line the first time, then nothing more:
--             the count next to the message in the row goes up instead. Never suspended.
--
--   "slow"  - on_tick burns 6 ms a frame. Expect the cost chip ("6.0 ms") on the row within a second,
--             then a suspension a couple of seconds later: "its callbacks averaged 6.0 ms per frame
--             for 120 frames". Your frame rate recovers the moment it is suspended.
--
--   "late"  - tries to register an on_frame from inside on_frame, every frame. Expect ONE error in
--             the row - "tw.on_frame called from a callback ... ignored" - with a count that climbs,
--             one notefeed line, and no suspension: a refused registration is not a crash.
--
--   "mute"  - mutes the game's song-title banner (XX_gui Do_ShowSongName), then starts throwing
--             after 10 s. Start a run within those 10 s: no title. Start another after the
--             suspension: the title is back, because a suspended script's mutes are lifted.
--             Switch puzzlepro_hud.lua off for this one - it mutes the same banner, and it is
--             not suspended.
--
-- Whatever the mode, the probe also says what it is doing in the ways a script can: a label drawn
-- from the very first frame, over the loading screen (on_frame with before_ready), a `waiting` state
-- during the grace (tw.pending), and a notefeed line when it is switched off (tw.on_unload).

local MODE = "throw"

local started = os.clock()
local label = "Fault probe: " .. MODE

local function elapsed()
    return os.clock() - started
end

-- Drawn before the game is up, to check that before_ready does what it says: this should be on the
-- loading screen, and nothing from any other script should be.
tw.on_frame(function()
    local x0, y0 = tw.hud.safe()
    tw.hud.text(x0 + 24, y0 + 8, label .. (tw.ready() and "" or "  (loading)"), tw.theme("text_warning"))
end, { before_ready = true })

tw.on_unload(function()
    tw.notify("Fault probe switched off")
end)

if MODE == "throw" then
    tw.on_frame(function()
        if elapsed() < 3 then
            tw.pending("grace period, " .. math.ceil(3 - elapsed()) .. " s left")
            return
        end
        error("thrown on purpose (MODE = throw)")
    end)

elseif MODE == "rare" then
    local next_at = 10
    tw.on_frame(function()
        if elapsed() >= next_at then
            next_at = next_at + 10
            error("thrown on purpose, once every 10 s (MODE = rare)")
        end
    end)

elseif MODE == "slow" then
    tw.on_tick(function()
        local t = os.clock()
        while os.clock() - t < 0.006 do end
    end)

elseif MODE == "late" then
    tw.on_frame(function()
        tw.on_frame(function() end)
    end)

elseif MODE == "mute" then
    tw.mute("XX_gui", "Do_ShowSongName")
    tw.on_frame(function()
        if elapsed() < 10 then
            tw.pending("muting the song title, throwing in " .. math.ceil(10 - elapsed()) .. " s")
            return
        end
        error("thrown on purpose (MODE = mute)")
    end)

else
    tw.error("unknown MODE: " .. tostring(MODE))
end
