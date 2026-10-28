-- ---------------------------------------------------------------------------------------------
-- What is playing (DESIGN.md §12)
-- ---------------------------------------------------------------------------------------------
--
-- The title and artist the song badge shows, cleaned of what the game and the player's files put in
-- them that is not part of the name. Read when a run starts and, while the HUD is up, twice a second
-- (main.lua): the strings change once per song, and a read converts a wide string to a new Lua string
-- every time.

local ch = require("channels")

local song_title, song_artist = ch.song_title, ch.song_artist

-- `revision` moves whenever either string changes, so whatever is measured from them is measured again.
local song = { title = nil, artist = nil, revision = 0 }

-- Tags are part of CurrentSongTitle, not a field of their own. Some come from the file - authors put
-- "[as-monoonly]" in a title to make the game play it as Mono - and some the game appends itself when it
-- loads the song: XX_StartHere #6835... writes `CurrentSongTitle := Unicode_Merge(CurrentSongTitle,
-- " [as-steep]")`, and the same for [as-portal]. A real title arrives as
-- "Drive my Life [as-steep] [as-prows1] [as-monoonly] [as-allgrey]". They are rules, not the name, and
-- what they do to the run is on screen already - the colour table, the feats.
local function clean_title(text)
    text = text:gsub("%s*%[as%-[^%]]*%]", "")
    return (text:gsub("^%s+", ""):gsub("%s+$", ""))
end

-- What VisMusic writes when a file has no artist tag (#252) - not a name, and printing it would be
-- printing the absence of one.
local NO_ARTIST = { ["Unknown Artist"] = true }

local function refresh()
    local title = song_title:get()
    if title == nil then
        return
    end
    title = clean_title(title)
    local artist = song_artist:get() or ""
    artist = (artist:gsub("^%s+", ""):gsub("%s+$", ""))
    if NO_ARTIST[artist] then
        artist = ""
    end

    if title ~= song.title or artist ~= song.artist then
        song.title, song.artist = title, artist
        song.revision = song.revision + 1
    end
end

-- A new song is loaded by the time the run-start events fire, so the badge has the right name from the
-- first frame of the run rather than from the next periodic read.
tw.on_call("StatCollector", "Do_ResetStats", "after", refresh)
tw.on_call("StatCollector", "Do_ResetSimpleStats", "after", refresh)

song.refresh = refresh

return song
