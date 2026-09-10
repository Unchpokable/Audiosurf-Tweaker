-- @name        PuzzlePRO HUD
-- @author      Audiosurf Tweaker
-- @version     1.4
-- @description HUD Replacer for real puzzle players. Real-time all-color statistics tracker, chain multiplier and chain drop timer read straight out of the game, live skill rating calculation. Get GUD, Get PuzzlePRO HUD
--
-- A run tracker that replaces several pieces of the game's own HUD rather than sitting next to
-- them. Everything it draws is measured, themed and positioned against the viewport; nothing is
-- hardcoded except padding.
--
-- The interesting parts are documented where they happen. Seven things are worth reading first,
-- because they are the reasons this is not the obvious script.
--
--
-- 1. WHAT IT MEASURES - by track ROW, not by collection EVENT
--
-- The obvious tracker counts TrafficCommander::Do_CollectCar (#763) and divides by the generated
-- track. That over-counts, visibly: on a colour a good player clears to the last block it goes past
-- 100%, by roughly the number of power-ups on the track.
--
-- The cause is not that the game subtracts anything. It adds a car that is not in the track.
--
--     IfElse #985:  QuestionBoxState[CurrentTraffic] > 0 && !DumptyScoopDown?
--                       -> Do_CollectQuestionBox (#986)     -- Do_CollectCar never runs
--                   else
--                       -> Do_CollectCar (#763)
--
-- and #986 ends with:
--
--     Traffic.Type[CurrentTraffic] := -1
--     BirthID := CurrentTraffic
--     call Do_BirthCar                       -- the INNER one (#207), not the wrapper (#756)
--
-- The wrapper is where the bookkeeping lives: it advances Index_TrafficPattern to the next row,
-- bumps NextStatCarToBirth, writes Traffic.PrecalcedArrayNum, and range-checks against
-- TotalCarCount. Bypassing it births a *duplicate of whatever row the spawner used last*, into the
-- slot the power-up just vacated - a real, collectible car with no row of its own. That row is
-- still spawned again later, normally.
--
-- So each power-up costs the colour tally -1 on the power-up's own colour (its collection never
-- reaches Do_CollectCar) and +1 on the duplicated row's colour. The total is right; the per-colour
-- split is not.
--
-- Both halves are fixed by counting rows instead of events. Do_CollectCar hands us the row id for
-- free - it is the second thing the handler does:
--
--     TrafficType := Traffic.Type[CurrentTraffic]
--     TrafficID   := Traffic.PrecalcedArrayNum[CurrentTraffic]
--
-- A legitimately born car's id is unique; a ghost carries the id of the power-up that vacated its
-- slot, which we have already spent. So: hook both handlers, count each row once, and the ghosts
-- fall out by themselves. See count_row().
--
-- What this deliberately does NOT do is de-duplicate the bonus prediction. Achievements::
-- Do_ReportBlockHit (#1088) does not de-duplicate either - it increments {colour}Hit and
-- TotalBlocksHit unconditionally - and those are the counters Do_CalculateFinalStats copies into
-- CollectedColorCounts and tests at >= 0.95 for Butter Ninja and Seeing Red. The prediction has to
-- count what the game will count, ghosts and all, so it reads those channels directly.
--
--
-- 2. WHERE THE DENOMINATORS COME FROM - the live pattern, and WHEN to re-read it
--
-- The track is StatCollector::Stats: TrafficPattern (#57): one row per block, colour id in x, ring
-- in y, lane in z. Bucketed the way Achievements::Do_GetTrafficCounts buckets it (see BUCKET), that
-- walk *is* the game's {colour}Total - the number Do_CalculateFinalStats divides by for the 95%
-- Butter Ninja / Seeing Red test. So walking it is not second-guessing the game; it is computing
-- the same number early.
--
-- Two other sources look easier and are both wrong, which cost two rounds of this:
--
--   * **Stats: TrafficColorCounts is not it.** Do_CountColors (#940) fills that at track generation
--     - and white and wild blocks do not exist yet at that point. They are placed afterwards, by
--     Do_ReseteWhiteWildBlocks (#1156), which **overwrites already-coloured rows**:
--
--         Do_RestoreToOriginalState : every row back to Stats: Old Color / Old Lane
--         Do_PlaceWhites (#1170)    : NumWhites random rows with x < 6  ->  x := 7
--         Do_PlaceWilds  (#1171)    : NumWilds  random rows with x < 6  ->  x := 6.1
--         Do_GeneratePowerups(#1528): QuestionBoxState per row, seeded by the song title
--
--     So TrafficColorCounts still counts a red that has since become a white. It is too big on
--     every colour and exactly zero on white - which is why the white column read "--" and why
--     Seeing Red went unpredicted on tracks with few reds, where losing one to a white is the
--     difference. The game gets this right because Do_GetTrafficCounts re-walks the live pattern at
--     the end of the run, and Do_CalculateFinalStats then overwrites TrafficColorCounts with the
--     result - which is the couple of frames of the numbers visibly correcting themselves just
--     before the results screen.
--
--   * **"Rescan when TotalCarCount changes" is not it either.** Nothing above changes the number of
--     blocks. Neither does Do_CharacterAndLeagueTrafficMods, which recolours the track for Mono,
--     Casual and Pro (Do_NinjaMagic, Do_TurnYellowRed, Do_TurnPurplesBlue,
--     Do_LimitTo4ColorsForProMode). The count is the one thing that does not move.
--
-- The rescan is therefore driven by the events that mean "the track is now what it will be", never
-- by watching a number - see the hooks below.
--
--
-- 3. WHAT IT TAKES OVER
--
--     XX_gui::Do_LadderMedalRequirements (#3464)  the medal dashboard
--     XX_gui::Do_ShowSongName            (#2955)  the song title
--     XX_gui::Timer1                     (#3938)  the chain bar
--
-- All three are pure render branches, and in each case the choice of node is the entire safety
-- argument:
--
--   * #3464 - rings, cover shape, celebration and flash timers hang off it and nothing else does;
--     nothing downstream reads a value it produces.
--   * #2955 - writes only SongnameX / SongnameEmissive / SongNamePositionSet?, and no other group
--     in the project imports any of the three.
--   * #3938 - Timer1 is the chain bar. Its sibling Timer2 (#3970) under the same handler is the
--     song progress bar, which is why this mutes the object and NOT Do_ShowChainBar (#398) - that
--     would take the progress bar with it.
--
-- Muting is also the right tool for the song title rather than driving the Tweaker's own
-- hidden_song_title tweak: a script has no access to the host's tweak state, and even with access
-- it would be sharing a toggle with the user. A mute needs no coordination - if the tweak is on,
-- the game's own If (#6378) already skips the call; if it is off, the mute catches it - and it is
-- removed the moment the script is disabled.
--
--
-- 4. THE CHAIN MULTIPLIER
--
-- Puzzle::ChainCount (#468) counts consecutive collections, capped at 100. What it is worth is not
-- linear in it: Do_SetMoneyMultiplier (#94) does
--
--     ScoreBonus := 1
--     if MultipleColorsMatched? : ScoreBonus += 0.3
--     if ChainCount > 0         : ScoreBonus += Envelope#643(ChainCount)
--
-- and Envelope #643 is a six-key curve, 0/1/4/10/20/100 -> 0/0.5/1.5/2/2.5/3. So the displayed
-- multiplier runs 1.0 to 4.0, and the bonus behind it runs 0 to 3.
--
-- That curve is also the answer to "how do I put a logarithmic scale on a linear colour gradient":
-- the game has already linearised it. The indicator's colour walks the block palette one stop per
-- key, so the colour moves at the rate the bonus actually grows.
--
-- The bar length is not this script's invention either. The game's own chain bar is
--
--     XX_gui::Timer1 (#3938) width = Inertia( (15.1041/3) * Envelope#12477(ChainCount) )
--
-- - the same six-key curve, and **linear in the bonus rather than in ChainCount**. That is the part
-- worth copying: a bar that grows with the bonus is the one the player has spent every previous run
-- learning to read at a glance. What is not copied is the constant. The game's own is a fixed world
-- length; a rail here is a sixteenth of the viewport per point of bonus, because a bar tuned to look
-- right at 1280 is a smear at QHD. The Inertia is why the game's slides rather than snaps; here that
-- is roll().
--
--
-- 5. THE CHAIN DROP TIMER
--
-- The chain is not held for a while and then released by a timer of its own. What exists is the
-- match collection pass, and the chain is whatever survives it:
--
--     Do_CollectMatchesOnTimer (#113) -> Do_ManageCollectionTimer (#455)
--
--         MatchTimer := (HintmanEliteFreezingMatchCollection? || BlocksSlidThisFrame?)
--                           ? MatchTimer
--                           : MatchTimer + StartGroup::PausableTickCount
--
--         if MatchTimer > SpecialPurpose::MatchCollectionTicks:
--             MatchTimer := 0
--             ...find matches...
--             if any        -> score them, Do_IncrementChainCount (#1437)
--             if none       -> Do_GraduallyResetChain (#1500)
--
-- So the countdown to "the chain is judged" is MatchCollectionTicks - MatchTimer, and it freezes
-- while blocks are sliding - which is why the number holds still through a collapse. That freeze is
-- also why the remaining time is read from the game every frame rather than timed here: a clock of
-- our own would keep running through it and promise time the player does not have.
--
-- Ticks are 25ths of a second. StartGroup::PausableTickCount is Quest3D's TickCount gated on the
-- pause screen (Stage/XX_Timer.cgr #0), and every second-valued counter in the game divides it by
-- 25 - StatCollector::Timer is `Timer + PausableTickCount/25`, and FirstPersonGracePeriod, the
-- constant it is compared against, is 5.
--
-- What losing the pass costs is the league's business rather than the timer's - ChannelSwitch #1467
-- on LeagueID: 0 -> the chain is gone, 1 -> FLOOR(chain/2), 2 -> gone. Only Pro halves it.
--
-- MatchCollectionTicks is per character and per league rather than a constant: 10/10/20 by league,
-- overridden to 15 for Easy Ninja, 17 for Ninja Pro, 25 for Eraser Elite, 28 for Ninja Mono, 35 for
-- Berserker, 50 for Freeride, and to whatever a song's own tag asks for. Reading the channel rather
-- than assuming a number is the entire reason the bar is right for every character.
--
--
-- 6. WHY EVERYTHING MOVES THROUGH ONE FUNCTION
--
-- Every number and every bar here is drawn from roll(), a keyed tween sampled through tw.ease.
-- Two things follow, and both are why it is not done per widget:
--
--   * retargeting mid-flight is the normal case rather than the exception. Points arrive in bursts,
--     the chain can jump two tiers in one pass, and a tween that has to finish before it accepts a
--     new target falls visibly behind the game. roll() restarts from wherever the value has got to.
--   * the chain's colour, its rail length and its printed multiplier are all functions of ONE
--     tweened bonus, so they cannot disagree. Tweening the three separately is three ways for the
--     colour to arrive before the bar.
--
-- The drop timer is deliberately NOT tweened. It is a countdown; smoothing it would mean showing a
-- number that is not the time left.
--
--
-- 7. THE SKILL RATING IS THE GAME'S OWN NUMBER
--
-- The game never shows it. It computes it at the end of a run and puts it on the wire, so a HUD that
-- gets it wrong is wrong invisibly - the player just concludes they had a bad run. Every term below
-- is therefore taken from the graph rather than reasoned about.
--
--     Achievements::Do_CalcSkillRating (#1189), inside Do_FinalizeAndStringEncodeExtendedRideStats
--     (#1118), which Do_CalculateFinalStats calls:
--
--         SkillRating := MAX(1, ROUND( PointsWithGridBonus / GoldRequirement * 100
--                                                          * (LeagueID + 1) ))
--
--     StatCollector::Do_FindAddBonusPoints (#797):
--
--         BonusPoints           += Points * <scaler>   per feat earned
--         BonusPoints_CleanOnly += Points * GridBonusMultiplyer   if NumTilesInGrid == 0
--         PointsWithGridBonus    = ROUND(Points) + ROUND(BonusPoints) + ROUND(BonusPoints_CleanOnly)
--
-- Three things about that are worth stating because each was a candidate for the figure being wrong
-- and each turned out not to be:
--
--   * **Every scaler is over the raw Points**, not compounded. So the whole thing is
--     `Points * (1 + sum of scalers)` and a script may add them up.
--   * **GoldRequirement is not a constant of the song.** XX_StartHere::Do_CalcMedalRequirements
--     (#4267) sets it to `TotalCarCount * {10, 30, 35}[LeagueID]`. Read the channel.
--   * **LeagueID is a property of the character**, written by XX_WindowState::Do_SetCharacter (#307)
--     as 0, 1 or 2 - not of the difficulty, which is ChosenDifficulty and drives a different set of
--     medal requirements entirely. `LeagueID + 1` is the 1/2/3 multiplier.
--
-- What WAS wrong was the last clause of the middle line. `NumTilesInGrid == 0` is tested **at the
-- moment the game scores**, so the Clean Finish bonus belongs to the player for as long as the board
-- is clear - it is not an optimistic extra. Showing it only as a parenthesised maybe under-reported
-- the headline by a fifth for most of a good puzzle run.

-- ---------------------------------------------------------------------------------------------
-- Palette
-- ---------------------------------------------------------------------------------------------
--
-- The colours the player actually sees come from XX_StartHere::FetchColorByID (#882), and there is
-- more than one palette behind it:
--
--     #1533  switch (XX_gui::Color5.x + .y + .z) == 0
--      +- 1 -> #1535  StartGroup #1542/#1538/#1534/#1530/#1526      "fallback"
--      +- 0 -> #7157  switch SpecialPurpose::isMechMode
--              +- 1 -> #7159  Highway::MechColor0..4                "mech"
--              +- 0 -> #7072  switch SpecialPurpose::PortalMode?
--                      +- 1 -> #7074  PlayerCar_Sword::PortalColor1..5   "portal"
--                      +- 0 -> #440   XX_gui::Color1..Color5             "normal"
--
-- Reading a channel and getting a plausible colour is not evidence the game is using it - a
-- Value Vector carries no "someone reads me" flag. The only way to tell a live branch from a spare
-- is to walk the switches, which is what palette_source() does.
--
-- Note the reversed numbering in the normal branch (ColorID 0 -> Color5) and the extra switch on
-- ids 2 and 3, which become GlobalMonoColor under Ninja or Freeride. Both are the game's, not a
-- transcription slip.
-- One row per bucket of BUCKET below, keyed by the colour id the palette should draw for it.
local COLOURS = {
    { id = 0, name = "purple", fallback = 0xFFE08CC8 },
    { id = 1, name = "blue", fallback = 0xFFE0C060 },
    { id = 2, name = "green", fallback = 0xFF60C060 },
    { id = 3, name = "yellow", fallback = 0xFF40D0E0 },
    { id = 4, name = "red", fallback = 0xFF5050E0 },
    -- "White" is the game's word for the row, not a description of what is in it: the switch below
    -- puts both Wild (6) and Stone (7) here. The swatch shows Stone, which is the common one.
    { id = 7, name = "white", fallback = 0xFFE0E0E0 },
}

-- Raw colour id -> the row it belongs to. This is ChannelSwitch #1248 in
-- Achievements::Do_GetTrafficCounts, read off the graph case by case:
--
--     0 -> PurpleTotal   1 -> BlueTotal   2 -> GreenTotal   3 -> YellowTotal
--     4 -> RedTotal      5 -> RedTotal    6 -> WhiteTotal   7 -> WhiteTotal
--
-- and no case 8 or above at all. An earlier version of this script had "7,8 white, 6 counted by
-- nobody", which is wrong at both ends: **Wild (6) counts as white** and **Bomb (8) is counted by
-- nobody**. That put every wild block on the track outside the denominator and every bomb inside
-- it.
--
-- Worth knowing but not fixable here: the game's *hit* switch (#1123 in Do_ReportBlockHit) does not
-- agree with this one. It maps 5 -> WildsUsed and 6 -> WhitesHit, and has no case 7 at all. So a
-- track carrying id-5 blocks has them in RedTotal but never in RedsHit, and the game's own Seeing
-- Red test cannot reach 95% on it. Predicting from the same two channels the game tests means
-- inheriting that, which is correct: the job is to predict what the game will do.
local BUCKET = { [0] = 0, [1] = 1, [2] = 2, [3] = 3, [4] = 4, [5] = 4, [6] = 7, [7] = 7 }

local YELLOW, RED = 3, 4

-- Medal colours are semantic rather than chrome, so they stay literal; everything else comes from
-- the overlay's palette so the widget belongs to Tweaker rather than merely sitting on top of it.
local BRONZE = 0xFF4E7DCD
local SILVER = 0xFFC8C8C8
local GOLD = 0xFF3FD5F5
local WHITE = 0xFFFFFFFF

-- In tier order, which is also the order of the three marker dots under the medal rail.
local MEDALS = { BRONZE, SILVER, GOLD }
-- Built once: these are roll() keys, looked up every frame, and building them by concatenation in
-- the draw loop would put three throwaway strings a frame on the collector for no reason.
local DOT_KEYS = { "medal_dot_bronze", "medal_dot_silver", "medal_dot_gold" }

-- ---------------------------------------------------------------------------------------------
-- Channels
--
-- Every handle is created once, here, and resolves itself lazily. Creating one inside on_frame
-- would rescan the whole group every frame forever - see Docs/scripting/channels.md.
-- ---------------------------------------------------------------------------------------------

-- Run state. Two reads, not a state machine fed by hooks: a flag assembled from transitions is only
-- correct if it saw every one of them, and this script can be enabled mid-run.
local startup_state = tw.float_ch("StartGroup", "StartupState")
local gameplay_state = tw.float_ch("StartGroup", "State_Gameplay")
local paused = tw.float_ch("XX_PauseScreen", "GamePaused?")

-- Addressed by index (#57) because "Stats: TrafficPattern" is not unique - there are four.
local traffic_pattern = tw.array_vec("StatCollector", 57, "Index_TrafficPattern")
local car_count = tw.float_ch("StatCollector", "TotalCarCount")
local points = tw.float_ch("StatCollector", "Points")
local best_match = tw.float_ch("StatCollector", "LargestMatch")
-- Live despite living in StatCollector: #14 has Puzzle::Fetch_NumberBlocksInPlay wired straight
-- into it, so it re-evaluates on every read rather than holding the end-of-run snapshot.
local tiles_left = tw.float_ch("StatCollector", "NumTilesInGrid")

local bronze_at = tw.float_ch("StartGroup", "BronzeRequirement")
local silver_at = tw.float_ch("StartGroup", "SilverRequirement")
local gold_at = tw.float_ch("StartGroup", "GoldRequirement")
local league_id = tw.float_ch("StartGroup", "LeagueID")

local scaler = {
    match7 = tw.float_ch("StatCollector", "Match7 Bonus Scaler"),
    match11 = tw.float_ch("StatCollector", "Match11 Bonus Scaler"),
    match21 = tw.float_ch("StatCollector", "Match21 Bonus Scaler"),
    yellow = tw.float_ch("StatCollector", "YellowNinjaBonusPoints"),
    red = tw.float_ch("StatCollector", "RedNinja Bonus Scaler"),
    clean = tw.float_ch("StatCollector", "GridBonusMultiplyer"),
    stealth = tw.float_ch("StatCollector", "PerfectNinjaMultiplier"),
}

local hits = {
    [YELLOW] = tw.float_ch("Achievements", "YellowsHit"),
    [RED] = tw.float_ch("Achievements", "RedsHit"),
}

local is_ninja = tw.float_ch("SpecialPurpose", "Ninja?")
local is_freeride = tw.float_ch("SpecialPurpose", "Freeride?")
local is_mech = tw.float_ch("SpecialPurpose", "isMechMode")
local is_portal = tw.float_ch("SpecialPurpose", "PortalMode?")
-- Set to 1 at both resets when the character is a Ninja, cleared in Do_ReportCarCollected the
-- moment a stone block reaches the grid. Live during the run, so Stealth is predictable.
local perfect_ninja = tw.float_ch("StatCollector", "PerfectNinjaRun?")
-- Song-title tags ([as-monoonly] and friends) can forbid Stealth and cumulative points.
local prevent_mono = tw.float_ch("SpecialPurpose", "PreventMonoStealthAndCumulativePoints?")

-- The index is not a typo: TrafficCommander has two Value channels called TrafficType (#631 and
-- #900), and a by-name lookup finds the first, which is not the one the collision handlers write.
local type_ch = tw.float_ch("TrafficCommander", 900)
-- These three ARE unique by name.
local traffic_id = tw.float_ch("TrafficCommander", "TrafficID")
local current_traffic = tw.float_ch("TrafficCommander", "CurrentTraffic")
local precalc_row = tw.array("TrafficCommander", "Traffic: PrecalcedArrayNum", "CurrentTraffic")

local scooping = tw.float_ch("SpecialPurpose", "DumptyScoopDown?")
local erasing = tw.float_ch("SpecialPurpose", "ShatterStorming?")

local chain_count = tw.float_ch("Puzzle", "ChainCount")
-- The game's own chain curve, read rather than reimplemented. Envelope is an Aco_FloatChannel
-- descendant, so a numeric accessor reaches it and evaluating it applies the curve to whatever
-- ChainCount currently holds. Addressed by index because it has no name of its own.
local chain_curve = tw.float_ch("Puzzle", 643)

-- The chain drop timer (header §5). Both names are unique in their own groups, so neither needs an
-- index - and neither should get one: MatchCollectionTicks is written from a dozen places and an
-- index would make the read more fragile without making it more precise.
local match_timer = tw.float_ch("Puzzle", "MatchTimer")
local match_ticks = tw.float_ch("SpecialPurpose", "MatchCollectionTicks")

-- Quest3D counts in 25ths of a second, and the game converts with a bare `/25` everywhere it wants
-- seconds. This is that 25.
local TICKS_PER_SECOND = 25

-- ---------------------------------------------------------------------------------------------
-- Palette resolution
-- ---------------------------------------------------------------------------------------------

-- Handles for palettes other than the normal one are created on demand. Two of those groups
-- (PlayerCar_Sword, and Highway before a run) are not always loaded, and a handle for an absent
-- group eventually reports itself in the notification feed - which would be noise, not information,
-- for a branch the player is not in.
local sources = {}

local function vectors(key, group, names)
    local made = sources[key]
    if made == nil then
        made = {}
        for i, name in ipairs(names) do
            made[i] = tw.vector_ch(group, name)
        end
        sources[key] = made
    end
    return made
end

-- ColorID -> channel, per branch. Index 6 in each list is the white/stone block, which is the same
-- StartGroup channel everywhere.
local function normal_palette()
    return vectors("normal", "XX_gui", { "Color5", "Color4", "Color3", "Color2", "Color1" })
end

local function stone()
    return vectors("stone", "StartGroup", { 2667 })[1]
end

local function mono_colour()
    return vectors("mono", "StartGroup", { 862 })[1]
end

-- Which of the four palettes FetchColorByID is actually reading right now.
local function palette_source()
    local r, g, b = normal_palette()[1]:get()
    -- The game's own test, verbatim: an unset Color5 means the player has configured no colours and
    -- the whole switch falls through to the spare palette in StartGroup.
    if r == nil then
        return "unknown"
    end
    if r + g + b == 0 then
        return "fallback"
    end
    if (is_mech:get() or 0) ~= 0 then
        return "mech"
    end
    if (is_portal:get() or 0) ~= 0 then
        return "portal"
    end
    return "normal"
end

local function palette_channels(source)
    if source == "fallback" then
        return vectors("fallback", "StartGroup", { 1542, 1538, 1534, 1530, 1526 })
    elseif source == "mech" then
        return vectors("mech", "Highway", { "MechColor4", "MechColor3", "MechColor2", "MechColor1", "MechColor0" })
    elseif source == "portal" then
        return vectors("portal", "PlayerCar_Sword", { "PortalColor5", "PortalColor4", "PortalColor3", "PortalColor2", "PortalColor1" })
    end
    return normal_palette()
end

-- The palette is a player setting, so it is re-read periodically - but not every frame, because six
-- vector evaluations to answer a question that changes when someone opens the options screen is six
-- too many.
local function refresh_palette()
    local source = palette_source()
    if source == "unknown" then
        return
    end

    local channels = palette_channels(source)
    -- Only the normal branch has the mono override on ids 2 and 3 (#3602 / #4350); the spare and
    -- mech palettes wire those cases straight through.
    local mono = nil
    if source == "normal" and ((is_ninja:get() or 0) ~= 0 or (is_freeride:get() or 0) ~= 0) then
        local r, g, b = mono_colour():get()
        if r then
            mono = tw.rgb(r, g, b)
        end
    end

    for i, c in ipairs(COLOURS) do
        local channel = (c.id == 7) and stone() or channels[i]
        if mono ~= nil and (c.id == 2 or c.id == 3) then
            c.colour = mono
        elseif channel ~= nil then
            local r, g, b = channel:get()
            if r then
                c.colour = tw.rgb(r, g, b)
            end
        end
    end
end

for _, c in ipairs(COLOURS) do
    c.colour = c.fallback
    -- Built once rather than concatenated per frame per cell: twelve throwaway strings a frame is
    -- twelve more than a table of six fixed rows needs.
    c.key_total = "total:" .. c.name
    c.key_taken = "taken:" .. c.name
end

-- ---------------------------------------------------------------------------------------------
-- What the game stops drawing for as long as this script is loaded. Each handle is kept so it can
-- be put back with :off() without a reload.
-- ---------------------------------------------------------------------------------------------
local muted = {
    medals = tw.mute("XX_gui", "Do_LadderMedalRequirements"),
    song_name = tw.mute("XX_gui", "Do_ShowSongName"),
    -- By index on purpose: "Timer1" is unique in XX_gui but says nothing, and getting this one
    -- wrong takes the song progress bar off the screen instead. See the header, §3.
    chain_bar = tw.mute("XX_gui", 3938),
}

-- ---------------------------------------------------------------------------------------------
-- Run tally, counted by track row
-- ---------------------------------------------------------------------------------------------

local taken = {}
local by_route = { grid = 0, buffer = 0, erased = 0 }
-- Row ids already spent. A ghost (header §1) arrives carrying an id we have seen, which is exactly
-- what makes it identifiable.
local seen = {}
local powerups, ghosts = 0, 0

-- 0 hidden, 1 fully shown. Declared up here rather than beside the frame handler so the drawing
-- helpers below can close over it and fade themselves.
local shown = 0

local function reset_tally()
    for _, c in ipairs(COLOURS) do
        taken[c.id] = 0
    end
    by_route.grid, by_route.buffer, by_route.erased = 0, 0, 0
    seen = {}
    powerups, ghosts = 0, 0
end

reset_tally()

-- Counts one collected block against the track row it came from. Returns whether it counted.
--
-- An id we have already spent means a ghost: a car the game birthed into a freed slot without
-- advancing the pattern cursor, so it is a duplicate of a row that is still going to spawn on its
-- own. It has no row in the denominator and must not be counted into the numerator - but it is a
-- real block the player collected, so it is reported rather than discarded silently.
local function count_row(id, colour)
    local bucket = BUCKET[math.floor(colour + 0.5)]
    if bucket == nil then
        return false
    end

    -- No usable row id - the channel has not resolved yet, or the game left -1 there. Count the
    -- block rather than drop it: that is the old, over-counting behaviour, and losing accuracy is
    -- the right way for a missing discriminator to degrade. Losing blocks is not.
    if id ~= nil and id >= 0 then
        if seen[id] then
            ghosts = ghosts + 1
            return false
        end
        seen[id] = true
    end

    taken[bucket] = (taken[bucket] or 0) + 1
    return true
end

-- Three events, and all three are needed. They are declared after the scan below is defined, at the
-- bottom of this section.
--
--   Do_ResetStats (#1)          the track is being regenerated. Called only from
--                               Do_GenerateSongStats, i.e. when a new song loads.
--
--   Do_ReseteWhiteWildBlocks    the whites, wilds and power-ups are being (re)placed over the
--   (#1156, the typo is theirs)  generated colours. Called from XX_StartHere::Do_ReturnToStart,
--                               inside its OneTime, so once per entry into a run. **This is the
--                               event that says the track is finally what the player will see**,
--                               and missing it is what made the denominators wrong: white
--                               placement uses plain RAND, so it lands somewhere different on
--                               every single run of the same song.
--
--   Do_ResetSimpleStats (#345)  the run counters are being zeroed. Called from Do_ReturnToStart
--                               too, after the OneTime above, so it is also the safe backstop for
--                               "the pattern is settled". Restarting from the pause menu fires
--                               this and NOT Do_ResetStats - the track is not regenerated - and a
--                               tracker hooked only on the latter keeps counting into the previous
--                               run's tally and sails past 100%.

-- ---------------------------------------------------------------------------------------------
-- How much traffic of each colour the track carries: walk the live pattern (header §2).
--
-- Spread over frames on purpose. Each row costs a cursor write, a vector read and a cursor restore,
-- and a track can be several thousand rows - doing it in one frame would be a visible hitch at the
-- exact moment the player is starting a run.
--
-- The previous total stays on screen while a rescan runs. It is the previous run's answer, but the
-- alternative is blanking the table for a quarter second at every start, and on a restart the two
-- are usually the same number anyway.
-- ---------------------------------------------------------------------------------------------
local ROWS_PER_FRAME = 128

-- Which colour columns the table shows. Derived from the track rather than from a table of modes,
-- because the game recolours the track per character and league before the run and the pattern is
-- the result - StatCollector::Do_CharacterAndLeagueTrafficMods (#904), called from Do_ResetStats:
--
--     LeagueID == 1        Do_LimitTo4ColorsForProMode (#886)  green(2) -> blue(1)
--     ThinTraffic?/Ninja?  Do_TurnPurplesBlue (#877)           purple(0) -> blue(1)
--     Ninja?/Freeride?     Do_NinjaMagic (#403) rewrites EVERY row:
--                              x := (x > NinjaColorCutoff) ? 3 : Puzzle::StoneColorID
--     isMechMode           yellow -> red, blue -> green
--     PortalMode?          purple -> blue, yellow -> green, red -> green
--
-- So Casual runs three colours, Pro four, Mono exactly two - the yellow slot and the stone slot -
-- and a column for a colour the track does not carry is a column of dashes. Reading it off the
-- pattern gets all six cases and every future one for free; a mode table would get five of them and
-- then go quietly stale.
local function visible_columns(counts)
    local out = {}
    for _, c in ipairs(COLOURS) do
        if (counts[c.id] or 0) > 0 then
            out[#out + 1] = c
        end
    end
    -- No colours at all is a scan that went wrong, not a track without colours. Showing everything
    -- is the honest way to say "no idea" - collapsing to nothing would hide the fault.
    if #out == 0 then
        return COLOURS
    end
    return out
end

local totals = { ready = false, by_row = {}, running = false, at = 0, rows = 0, partial = {}, columns = COLOURS }

local function begin_scan()
    totals.running = true
    totals.at = 0
    totals.rows = math.floor((car_count:get() or 0) + 0.5)
    totals.partial = {}
    for _, c in ipairs(COLOURS) do
        totals.partial[c.id] = 0
    end
end

local function step_scan()
    if not totals.running then
        return
    end

    if totals.rows <= 0 then
        -- Asked before the track existed. Try again next frame rather than publishing zero.
        totals.rows = math.floor((car_count:get() or 0) + 0.5)
        if totals.rows <= 0 then
            return
        end
    end

    local budget = ROWS_PER_FRAME
    while totals.at < totals.rows and budget > 0 do
        local id, ring, lane = traffic_pattern:get(totals.at)
        -- Not resolvable yet (group still loading): stop, keep the position, try next frame.
        if id == nil then
            return
        end

        -- An all-zero row is not a block. Traffic thinning takes a car off the track by blanking its
        -- whole row - Do_NeuterThisCar (#680) writes Value Vector#684, which is (0, 0, 0) - and it
        -- runs for every character with SpecialPurpose::ThinTraffic? as well as for every Ninja:
        --
        --     Ninja?    : x > 5 && (y - LastAllowedRing) < RequiredSeparation  -> neuter
        --     otherwise : x < 3 && (same separation test)                      -> neuter
        --
        -- The row stays in the array, so a walk that only looks at x counts it as colour id 0 and
        -- reports a purple column full of blocks nobody can collect. The non-Ninja branch hides
        -- this by accident: Do_TurnPurplesBlue (#877) runs on the same row a moment later and turns
        -- the zero into a blue. The Ninja branch has no such step, which is why Mono was the mode it
        -- showed up in.
        --
        -- Testing all three components rather than just the colour is the point: a genuine purple
        -- block has a ring, and the game's own count (Do_GetTrafficCounts) makes exactly this
        -- mistake - its switch files id 0 under PurpleTotal without looking any further. Dropping
        -- these rows cannot move the bonus prediction, because neutering never touches yellow or
        -- red in either branch.
        if id ~= 0 or (ring or 0) ~= 0 or (lane or 0) ~= 0 then
            -- floor, not round: Do_PlaceWilds writes 6.1, and the engine's own switch truncates.
            local bucket = BUCKET[math.floor(id)]
            if bucket then
                totals.partial[bucket] = totals.partial[bucket] + 1
            end
        end

        totals.at = totals.at + 1
        budget = budget - 1
    end

    if totals.at >= totals.rows then
        totals.running = false
        totals.by_row = totals.partial
        -- Settled once per scan, not per frame: the column set is layout, and layout that is
        -- recomputed while the player is looking at it is layout that moves.
        totals.columns = visible_columns(totals.partial)
        totals.ready = true
    end
end

-- The hooks described above. A hook runs on the game's own call stack, so all it does is arm the
-- scan; the walking happens in on_frame.
local function run_started()
    reset_tally()
    begin_scan()
end

tw.on_call("StatCollector", "Do_ResetStats", "after", run_started)
tw.on_call("StatCollector", "Do_ReseteWhiteWildBlocks", "after", run_started)
tw.on_call("StatCollector", "Do_ResetSimpleStats", "after", run_started)

-- ---------------------------------------------------------------------------------------------
-- Collection events
-- ---------------------------------------------------------------------------------------------

-- "after": TrafficType and TrafficID are the first two things the handler writes, so both are valid
-- by the time this runs, and so are the two ability flags that say which branch was taken.
tw.on_call("TrafficCommander", "Do_CollectCar", "after", function()
    local colour = type_ch:get()
    if colour == nil then
        return
    end

    if not count_row(traffic_id:get(), colour) then
        return
    end

    local scoop = scooping:get()
    local erase = erasing:get()
    if scoop and scoop ~= 0 then
        by_route.buffer = by_route.buffer + 1
    elseif erase and erase ~= 0 then
        by_route.erased = by_route.erased + 1
    else
        by_route.grid = by_route.grid + 1
    end
end)

-- The other half of the funnel. A power-up block never reaches Do_CollectCar, so without this the
-- tally is short one block of that colour for every power-up taken.
--
-- Both reads are still valid in the "after" phase: TrafficType is written before the rebirth, and
-- the rebirth writes Traffic.Type[slot] rather than the TrafficType channel. CurrentTraffic was put
-- back on the power-up's slot by Set Value #1201, and the inner Do_BirthCar does not touch
-- PrecalcedArrayNum - that lives in the wrapper it bypassed.
tw.on_call("TrafficCommander", "Do_CollectQuestionBox", "after", function()
    local colour = type_ch:get()
    if colour == nil then
        return
    end

    local slot = current_traffic:get()
    local id = slot and precalc_row:get(slot) or nil

    powerups = powerups + 1
    count_row(id, colour)
end)

-- ---------------------------------------------------------------------------------------------
-- The chain multiplier
-- ---------------------------------------------------------------------------------------------

-- Envelope #643's keys, lifted out of its ENV1 chunk. Kept here for two reasons: as the fallback
-- when the channel cannot be read, and as a running cross-check on the channel - an index is the
-- most fragile way to address anything, and a wrong one here would show a plausible wrong number
-- rather than failing.
local CHAIN_KEYS = { { 0, 0.0 }, { 1, 0.5 }, { 4, 1.5 }, { 10, 2.0 }, { 20, 2.5 }, { 100, 3.0 } }

-- A full bar. The game divides by this same 3 to scale its own (header §4), so a rail drawn at
-- bonus/3 of its maximum is the length the player already knows how to read.
local CHAIN_MAX = CHAIN_KEYS[#CHAIN_KEYS][2]
-- The first key worth anything: a chain of 1 is a bonus of 0.5. Below it the player has no chain,
-- which is what the drop timer's visibility hangs on.
local CHAIN_FIRST = CHAIN_KEYS[2][2]

local function chain_bonus_from_table(chain)
    if chain <= 0 then
        return 0
    end

    for i = 1, #CHAIN_KEYS - 1 do
        local x0, y0 = CHAIN_KEYS[i][1], CHAIN_KEYS[i][2]
        local x1, y1 = CHAIN_KEYS[i + 1][1], CHAIN_KEYS[i + 1][2]
        if chain <= x1 then
            return y0 + (y1 - y0) * (chain - x0) / (x1 - x0)
        end
    end

    return CHAIN_KEYS[#CHAIN_KEYS][2]
end

-- Starts trusting the channel and stops if it ever disagrees with the curve above. Cheap - one
-- extra float read per frame - and it turns "the index drifted in a patch" from a silently wrong
-- number into a message and a correct one.
local curve_trusted = true
local curve_doubts = 0

local function chain_bonus()
    local chain = chain_count:get()
    if chain == nil then
        return nil, nil
    end

    local expected = chain_bonus_from_table(chain)
    if not curve_trusted then
        return expected, chain
    end

    local live = chain_curve:get()
    if live == nil then
        return expected, chain
    end

    -- The curve interpolates its keys with a spline, this table with straight lines, so they differ
    -- slightly between keys. The tolerance is for that; an index pointing at an unrelated channel
    -- misses by far more.
    if math.abs(live - expected) > 0.35 then
        curve_doubts = curve_doubts + 1
        if curve_doubts > 30 then
            curve_trusted = false
            tw.warn("chain curve channel disagrees with the known keys - falling back to ChainCount")
        end
        return expected, chain
    end

    curve_doubts = 0
    return live, chain
end

-- How long until the pass that decides whether the chain survives, as the text to print and the
-- fraction of the window still to run (header §5).
--
-- Read from the game every frame rather than counted down here. The game's timer stops while blocks
-- are sliding and can be pushed backwards by Do_ResetMatchingTimer; a clock of our own would do
-- neither, and would promise time the player has not got.
local function drop_timer()
    local window = match_ticks:get()
    local elapsed = match_timer:get()
    if window == nil or elapsed == nil or window <= 0 then
        return nil, 0
    end

    local left = window - elapsed
    if left < 0 then
        -- The pass fires on the frame the window is exceeded, so this is only ever a frame wide.
        left = 0
    elseif left > window then
        -- Do_ResetMatchingTimer subtracts a whole window rather than clamping, which leaves the
        -- counter negative while match collection is suppressed - during a tutorial, or under a
        -- Hintman freeze.
        left = window
    end

    return string.format("%.2f", left / TICKS_PER_SECOND), left / window
end

-- ---------------------------------------------------------------------------------------------
-- Colour helpers
-- ---------------------------------------------------------------------------------------------

local function mix(a, b, t)
    if t <= 0 then
        return a
    end
    if t >= 1 then
        return b
    end

    local out = 0
    local scale = 1
    for _ = 1, 4 do
        local ca = a % 256
        local cb = b % 256
        out = out + math.floor(ca + (cb - ca) * t + 0.5) * scale
        a = math.floor(a / 256)
        b = math.floor(b / 256)
        scale = scale * 256
    end
    return out
end

-- Bonus 0 (within float noise) is white; above that the colour walks the block palette, one stop
-- per envelope key. Using the key ordinal as the gradient axis rather than the bonus itself is the
-- whole point: the bonus steps are 0.5/1.0/0.5/0.5/0.5, so a gradient keyed on the bonus would
-- crawl through one band and sprint through the rest. On the ordinal axis the colour advances once
-- per tier, and the game's own curve keeps the tiers where the game put them.
local function chain_colour(bonus)
    if bonus < 1e-4 then
        return WHITE
    end

    for i = 1, #CHAIN_KEYS - 1 do
        local y0 = CHAIN_KEYS[i][2]
        local y1 = CHAIN_KEYS[i + 1][2]
        if bonus <= y1 then
            local from = (i == 1) and WHITE or COLOURS[i - 1].colour
            local to = COLOURS[i].colour
            local span = y1 - y0
            return mix(from, to, span > 0 and (bonus - y0) / span or 1)
        end
    end

    return COLOURS[#CHAIN_KEYS - 1].colour
end

-- ---------------------------------------------------------------------------------------------
-- Animation
-- ---------------------------------------------------------------------------------------------

-- A named tween. Hand it a key and where the value should be, and it answers where the value is
-- right now - restarting from wherever it had got to if the target moved mid-flight (header §6).
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

-- ---------------------------------------------------------------------------------------------
-- Scoring
-- ---------------------------------------------------------------------------------------------

local function league()
    local id = league_id:get()
    if id == nil then
        return 1, "casual"
    end
    -- LeagueID is 0-based: XX_StartHere's Do_CalcMedalRequirements switches the medal scalers on it
    -- directly. The name is printed alongside the rating so a wrong reading is visible rather than
    -- silently folded into the number.
    local n = math.floor(id + 0.5) + 1
    if n < 1 then
        n = 1
    elseif n > 3 then
        n = 3
    end
    return n, ({ "casual", "pro", "elite" })[n]
end

-- Everything the game will add to Points at the end, as far as it is knowable mid-run, minus Clean
-- Finish - that one is shown separately as the second number, because it is only decided on the
-- last frame. Returns the multiplier and the feat names in the game's own order
-- (Do_FindAddBonusPoints #797).
local function earned_bonus()
    local mult, feats = 0, {}

    local ninja = (is_ninja:get() or 0) ~= 0
    local prevent = (prevent_mono:get() or 0) ~= 0

    -- Stealth. The whole Mono half of the scoring table used to be missing here, which made the
    -- prediction wrong by a third of the score for every Ninja character.
    if ninja and (perfect_ninja:get() or 0) ~= 0 and not prevent then
        mult = mult + (scaler.stealth:get() or 0.33)
        feats[#feats + 1] = "Stealth"
    end

    local lm = best_match:get() or 0
    if lm > 20 then
        mult = mult + (scaler.match21:get() or 0.21)
        feats[#feats + 1] = "Match21"
    elseif lm > 10 then
        mult = mult + (scaler.match11:get() or 0.11)
        feats[#feats + 1] = "Match11"
    elseif lm > 6 then
        mult = mult + (scaler.match7:get() or 0.07)
        feats[#feats + 1] = "Match7"
    end

    -- Both colour bonuses are gated on !Ninja? - a Ninja gets Stealth instead, and never these.
    --
    -- The 95% test uses Achievements::YellowsHit / RedsHit over Achievements' own totals, which is a
    -- different quantity from the de-duplicated table above (header §1). This reads what the game
    -- will read; the table shows the truth. Where they disagree is itself worth seeing.
    if not ninja and totals.ready then
        local yellow_total = totals.by_row[YELLOW] or 0
        local yellow_hit = hits[YELLOW]:get()
        if yellow_total > 0 and yellow_hit and yellow_hit / yellow_total >= 0.95 then
            mult = mult + (scaler.yellow:get() or 0.05)
            feats[#feats + 1] = "Butter Ninja"
        end

        local red_total = totals.by_row[RED] or 0
        local red_hit = hits[RED]:get()
        if red_total > 0 and red_hit and red_hit / red_total >= 0.95 then
            mult = mult + (scaler.red:get() or 0.05)
            feats[#feats + 1] = "Seeing Red"
        end
    end

    -- Worth no points, but the game puts it in the Feat String, so a tracker that omits it
    -- disagrees with the results screen for no reason.
    if prevent then
        feats[#feats + 1] = "Mono Special"
    end

    return mult, feats
end

local FEAT_ICON = {
    ["Clean Finish"] = "feat_clean_finish",
    ["Stealth"] = "feat_stealth",
    ["Match7"] = "feat_match7",
    ["Match11"] = "feat_match11",
    ["Match21"] = "feat_match21",
    ["Butter Ninja"] = "feat_butter_ninja",
    ["Seeing Red"] = "feat_seeing_red",
    ["Mono Special"] = "feat_mono_special",
}

-- ---------------------------------------------------------------------------------------------
-- Drawing
-- ---------------------------------------------------------------------------------------------

-- Which medal the score currently holds: 0 none, 1 bronze, 2 silver, 3 gold. The game's own test
-- (Do_CalculateMedalEarned #845) is three unordered comparisons against PointsWithGridBonus, which
-- is what the caller passes; a threshold the script could not read is skipped rather than assumed.
local function medal_tier(score, bronze, silver, gold)
    if gold ~= nil and gold > 0 and score >= gold then
        return 3
    end
    if silver ~= nil and silver > 0 and score >= silver then
        return 2
    end
    if bronze ~= nil and bronze > 0 and score >= bronze then
        return 1
    end
    return 0
end

-- Progress to gold, in the same language as the chain rails: a hairline, one colour, and light.
--
-- The rail carries ONE colour - the medal currently held - rather than three abutting zones. At the
-- thickness the rest of this HUD uses, three colours in a row do not read as three zones; they read
-- as a smear, which is exactly what the old bar had become. So the colour is the medal, the length
-- is the progress, and the three dots underneath are the discrete state.
--
-- The dots are indicators, not dividers. They sit at equal spacing, centred under the rail, and
-- deliberately say nothing about where the thresholds fall - putting them on the rail at their
-- score positions is what made the old bar look like a chart.
--
-- There is deliberately no overshoot indicator either. Gold is PointsWithGridBonus ==
-- GoldRequirement, so by Do_CalcSkillRating (header §7) gold is exactly 100 skill rating per league:
-- 100 casual, 200 pro, 300 elite. Anyone past gold is already reading that off the number, and a
-- second thing saying the same is a second thing to keep true.
local function draw_medal_rail(x0, y0, x1, h, dot, gap_y, score, bronze, silver, gold, stops, track, fade)
    local radius = h * 0.5
    local w = x1 - x0

    tw.hud.rect(x0, y0, x1, y0 + h, track, radius)

    -- The tier is tweened and the colour interpolated along the stops by that rolled ordinal, so
    -- earning a medal sweeps the rail from one metal to the next instead of switching it. Same trick
    -- as the chain gradient: one number moving, not two colours picked per frame.
    local tier = medal_tier(score, bronze, silver, gold)
    local t = roll("medal_tier", tier, 0.35)
    local lo = math.min(math.floor(t), #stops - 2)
    local colour = mix(stops[lo + 1], stops[lo + 2], t - lo)

    if gold ~= nil and gold > 0 then
        local fill = w * math.min(math.max(score / gold, 0), 1)
        -- Shorter than it is thick is a dot, not a bar - the same rule the chain rails use, and for
        -- the same reason: below that the rounding eats the shape.
        if fill > h then
            local c = tw.fade(colour, fade)
            tw.hud.rect(x0, y0, x0 + fill, y0 + h, c, radius)
            -- The glow rises with the tier, so the rail gains weight as the medals do rather than
            -- only changing hue.
            tw.hud.glow_rect(x0, y0, x0 + fill, y0 + h, c, radius, (0.3 + 0.2 * t) * fade)
        end
    end

    local spacing = math.max(dot * 2.5, w * 0.1)
    local cy = y0 + h + gap_y
    for i, medal in ipairs(MEDALS) do
        -- Lit is its own roll per dot rather than a slice of the tier roll: a medal lights up on its
        -- own clock, and sharing one would make the third dot start moving when the first was won.
        local lit = roll(DOT_KEYS[i], tier >= i and 1 or 0, 0.35)
        local cx = x0 + w * 0.5 + spacing * (i - 2)
        -- Unearned is the medal's own colour held down rather than a neutral, so the row reads as
        -- three named medals waiting rather than as three anonymous pips.
        local c = tw.fade(tw.alpha(medal, 0.16 + 0.84 * lit), fade)
        tw.hud.rect(cx - dot * 0.5, cy, cx + dot * 0.5, cy + dot, c, dot * 0.5)
        if lit > 0.01 then
            tw.hud.glow_rect(cx - dot * 0.5, cy, cx + dot * 0.5, cy + dot, c, dot * 0.5, 0.75 * lit * fade)
        end
    end
end

local function centered(x, width, y, text, colour, size, font)
    local w = tw.hud.measure(text, size, font)
    tw.hud.text(x + (width - w) * 0.5, y, text, colour, size, font)
end

-- Is the player actually playing right now? nil from either channel means "could not tell", and the
-- answer to that is yes: hiding a working widget because a lookup failed is a worse failure than
-- showing it somewhere it is not wanted.
local function in_run()
    local state = startup_state:get()
    local gameplay = gameplay_state:get()
    if state == nil or gameplay == nil then
        return true
    end
    if math.abs(state - gameplay) > 0.5 then
        return false
    end

    -- Only asked once we know we are in gameplay, which is also the only time XX_PauseScreen is
    -- certainly loaded - asking from the main menu would just accumulate "no such group" warnings.
    local held = paused:get()
    return (held or 0) == 0
end

-- Time-based rather than per-frame: the game does not run at a fixed frame rate, and a constant
-- step per frame fades at whatever speed the machine happens to reach.
local FADE_SECONDS = 0.18

tw.on_frame(function()
    if not tw.engine_ready() then
        return
    end




    -- Before the visibility gate: the track is generated and dressed while the loading screen is
    -- still up, so the walk has to be allowed to run before the run starts.
    step_scan()

    -- The one case the hooks cannot cover: this script was enabled part-way through a run, so the
    -- events that would have armed the scan are long past.
    if not totals.ready and not totals.running and tw.frame % 60 == 0 then
        begin_scan()
    end

    local target = in_run() and 1 or 0
    local step = tw.dt() / FADE_SECONDS
    if shown < target then
        shown = math.min(target, shown + step)
    elseif shown > target then
        shown = math.max(target, shown - step)
    end

    -- Fully faded out: nothing to draw, and nothing below this point should be evaluated either -
    -- the per-frame channel reads are only worth paying for when someone can see the result.
    if shown <= 0 then
        return
    end

    if tw.frame % 30 == 0 then
        refresh_palette()
    end

    -- Eased so the widget arrives and leaves with some weight instead of ramping linearly.
    local fade = tw.ease("cubicOut", shown)

    -- Everything structural comes from the overlay's theme, so the widget follows a theme change
    -- instead of drifting out of it. Everything goes through the fade, so entering and leaving a run
    -- dissolves the widget instead of popping it - including the block swatches, which is why those
    -- are faded at the point of use rather than here.
    local function themed(name, weight)
        local colour = tw.theme(name)
        if weight then
            colour = tw.alpha(colour, weight)
        end
        return tw.fade(colour, fade)
    end

    local RULE = themed("border_subtle")
    local TEXT = themed("text_primary")
    local DIM = themed("text_muted")
    local FAINT = themed("text_faint")
    local WARN = themed("text_warning")
    local TRACK = themed("control_track_off")

    -- Tier -> rail colour, index 1 being "no medal yet". That one is the overlay's own neutral
    -- rather than a fourth metal, so the rail starts grey and warms into bronze. Raw rather than
    -- themed(): the rail mixes between two of these and fades the result once, and fading twice
    -- would make the transition dip in the middle.
    local MEDAL_STOPS = { tw.theme("text_muted"), BRONZE, SILVER, GOLD }

    local base = tw.hud.font_size()
    local screen_w = tw.hud.size()

    local value_size = base * 1.15
    local label_size = base * 0.85
    local rating_size = base * 2.0
    local chain_size = base * 1.6

    local swatch = math.max(10, base * 0.8)
    local row_gap = 5
    local row_h = value_size + 6
    local cell_w = math.max(swatch + 14, base * 2.8)

    -- The medal rail is the chain rails' own thickness, on purpose: it was the one filled solid on a
    -- screen of hairlines, and matching the thickness is most of what stops it reading as a blot.
    local rail_h = math.max(3, base * 0.18)
    local medal_dot = math.max(4, rail_h * 1.6)
    local medal_gap = math.max(3, base * 0.22)
    local medal_h = rail_h + medal_gap + medal_dot

    local sx0, _, sx1, sy1 = tw.hud.safe()
    local margin = 16
    local bottom = sy1 - margin

    -- ---- the colour table, bottom-left ------------------------------------------------------
    -- Only the colours this track actually carries (see visible_columns): a Casual run has three,
    -- Pro four, Mono two, and a column of dashes for a colour that cannot appear is noise.
    local columns = totals.columns
    local table_w = cell_w * #columns
    local table_h = swatch + row_gap + row_h * 2
    local tx = sx0 + margin
    local swatch_y = bottom - table_h
    local totals_y = swatch_y + swatch + row_gap
    local taken_y = totals_y + row_h

    tw.hud.line(tx, totals_y - 2, tx + table_w, totals_y - 2, RULE, 1)
    tw.hud.line(tx, taken_y - 2, tx + table_w, taken_y - 2, RULE, 1)

    local got_total, all_total = 0, 0

    for i, c in ipairs(columns) do
        local cx = tx + cell_w * (i - 1)
        local total = totals.ready and totals.by_row[c.id] or nil
        local got = taken[c.id] or 0

        local colour = tw.fade(c.colour, fade)

        -- A small square with a soft halo rather than a bar filling the cell: at a glance the row
        -- reads as six markers over six numbers, not as a stacked chart.
        local sx0 = cx + (cell_w - swatch) * 0.5
        tw.hud.rect(sx0, swatch_y, sx0 + swatch, swatch_y + swatch, colour, 3)
        tw.hud.glow_rect(sx0, swatch_y, sx0 + swatch, swatch_y + swatch, colour, 3, 0.55 * fade)

        -- Both numbers roll to their new value rather than snapping to it. The rolled figure is for
        -- display only - every decision below (is there a total at all, is the colour finished) is
        -- made on the real one, so a colour does not light up early because its counter is still
        -- catching up.
        local shown_total = total and math.floor(roll(c.key_total, total) + 0.5)
        local shown_taken = math.floor(roll(c.key_taken, got) + 0.5)

        if total == nil or total < 1 then
            centered(cx, cell_w, totals_y + 3, "--", FAINT, value_size, "semibold")
            centered(cx, cell_w, taken_y + 3, total == nil and "--" or tostring(shown_taken), total == nil and FAINT or DIM,
                value_size, "semibold")
        else
            got_total = got_total + got
            all_total = all_total + total
            centered(cx, cell_w, totals_y + 3, tostring(shown_total), DIM, value_size, "semibold")
            -- A colour finished off the road reads as that colour rather than as plain text.
            centered(cx, cell_w, taken_y + 3, tostring(shown_taken), got >= total and colour or TEXT, value_size, "semibold")
        end
    end

    -- ---- skill rating and the medal bar, bottom-right ---------------------------------------
    local right_w = math.max(190, base * 14)
    local right_h = rating_size + 8 + medal_h
    local rx1 = sx1 - margin

    -- Pins float mid-height on one side and are the one piece of overlay chrome the safe area
    -- cannot express, so they are asked about separately.
    local px0, py0, px1, py1 = tw.hud.widget("pins")
    if px0 and px0 < rx1 and px1 > rx1 - right_w and py1 > bottom - right_h and py0 < bottom then
        rx1 = px0 - 12
    end

    local rx = rx1 - right_w
    local ry = bottom - right_h

    local score = points:get() or 0
    local gold = gold_at:get()
    local mult, league_name = league()

    local bonus, feats = earned_bonus()
    local clean = scaler.clean:get() or 0.25
    local grid_empty = (tiles_left:get() or -1) == 0

    -- What the game would put on the wire if the run ended on this frame (header §7).
    --
    -- The Clean Finish bonus is not a hypothetical the player might reach: the game adds it whenever
    -- the board is empty **at the moment it scores**, so while the board is clear it is already
    -- theirs. Leaving it out of the headline under-reported by a fifth exactly while a good player
    -- was holding a clean board - which is most of a good run, and the whole of the end of one.
    local live_bonus = bonus + (grid_empty and clean or 0)
    -- The other side of that: what the rating becomes if the board fills, or what it would become if
    -- it emptied. Whichever it is, it is the figure the player is not currently on.
    local alt_bonus = grid_empty and bonus or (bonus + clean)

    local now_points = score * (1 + live_bonus)
    local alt_points = score * (1 + alt_bonus)

    local function rating(p)
        if gold == nil or gold <= 0 then
            return 0
        end
        return p / gold * 100 * mult
    end

    -- Score arrives in bursts - a big match is worth thousands at once - so the rating is rolled
    -- rather than printed live. The two figures are rolled separately: they move by different
    -- amounts, and a single tween applied to both would drag the second one around by the first.
    local label = "SKILL RATING"
    local value = string.format("%.0f", roll("rating", rating(now_points)))
    local alt = string.format(" (%.0f)", roll("rating_alt", rating(alt_points)))

    local lw = tw.hud.measure(label, label_size)
    local vw = tw.hud.measure(value, rating_size, "semibold")
    local ow = tw.hud.measure(alt, label_size)

    -- One baseline for three different sizes, so the small text sits on the big text's bottom edge.
    -- Exact rather than eyeballed, which is the whole reason a script can measure at all.
    local text_x = rx + (right_w - (lw + 8 + vw + ow)) * 0.5
    local baseline = ry + rating_size

    tw.hud.text(text_x, baseline - label_size, label, DIM, label_size)
    tw.hud.text(text_x + lw + 8, baseline - rating_size, value, TEXT, rating_size, "semibold")

    -- Which of the two the second figure is is never in doubt - it sits above the headline in one
    -- case and below it in the other - so it is tinted by direction rather than labelled: an upside
    -- still to be had is quiet, a Clean Finish already banked and now at risk is not.
    tw.hud.text(text_x + lw + 8 + vw, baseline - label_size, alt, grid_empty and WARN or FAINT, label_size)

    -- The rolled score, not the raw one, so the rail and the figure it is a picture of arrive
    -- together - and so a dot lights on the frame the *displayed* number crosses its threshold. A
    -- dot that lit off the true score would light while the number above it still read short of it.
    local bar_y = ry + rating_size + 8
    draw_medal_rail(rx, bar_y, rx + right_w, rail_h, medal_dot, medal_gap, roll("medal", now_points), bronze_at:get(),
        silver_at:get(), gold, MEDAL_STOPS, TRACK, fade)

    -- ---- the chain, its drop timer and the feats, bottom-centre ------------------------------
    --
    -- Three rows on one centre, so they read as a column rather than as three things that happen to
    -- be near each other: the multiplier, the countdown to the pass that will judge it, and the
    -- feats. They share a centre and one rule - stay out of the colour table and the rating block -
    -- and nothing else. Their widths are independent on purpose.
    --
    -- The chain rails used to be sized from the feat row, which was a mistake worth recording: a
    -- feat can appear and vanish mid-run - Clean Finish flickers on every time the grid happens to
    -- empty - and the rails would jump a width the player had read as a chain reading. The two have
    -- nothing to do with each other, so they are no longer allowed to move each other.
    if grid_empty then
        table.insert(feats, 1, "Clean Finish")
    end

    local icon_size = label_size * 1.3
    local icon_gap = 5
    local feat_gap = 14

    local feats_w = 0
    for i, feat in ipairs(feats) do
        if i > 1 then
            feats_w = feats_w + feat_gap
        end
        feats_w = feats_w + icon_size + icon_gap + tw.hud.measure(feat, label_size)
    end

    -- Centred on the VIEWPORT, and kept clear of the two side blocks. Those are two different
    -- things and it used to do only the second: centring on the middle of the leftover strip put
    -- the block off-centre by half the difference between the table's width and the rating block's,
    -- which is visible, because the eye reads "centred" against the screen and not against whatever
    -- space happened to be left over.
    --
    -- The room it gets is the NEARER of the two sides, mirrored - taking each side's own distance
    -- would let the block grow lopsided about the centre it is supposed to be on.
    local free_l = tx + table_w + 24
    local free_r = rx - 24
    local block_cx = screen_w * 0.5
    local avail = 2 * math.min(block_cx - free_l, free_r - block_cx)

    -- One tween behind the colour, the length and the printed number, so the three cannot arrive at
    -- different times (header §6).
    local chain_gain = chain_bonus()
    local gain = chain_gain and roll("chain", chain_gain, 0.28) or nil
    local chain_text = gain and string.format("x%.1f", 1 + gain) or nil
    -- The timer row is a picture of what happens to the row above it, so it is not drawn without
    -- one: on its own it would be a countdown to nothing in particular.
    local timer_text, timer_frac = nil, 0
    if chain_text ~= nil then
        timer_text, timer_frac = drop_timer()
    end

    local timer_size = chain_size * 0.62
    local rail_gap = base * 0.7
    local chain_w, chain_h = 0, 0
    local timer_w, timer_h = 0, 0
    if chain_text ~= nil then
        chain_w, chain_h = tw.hud.measure(chain_text, chain_size, "semibold")
    end
    if timer_text ~= nil then
        timer_w, timer_h = tw.hud.measure(timer_text, timer_size, "semibold")
    end

    -- One gap wide enough for both numbers, so the two rows' rails line up in a column instead of
    -- each row indenting itself by whatever its own text happens to measure.
    local gap_w = math.max(chain_w, timer_w)

    -- A rail is a sixteenth of the viewport per point of bonus. A fraction of the viewport rather
    -- than a fixed pixel count because a bar that reads well at 1280 is a smear at QHD; per point of
    -- bonus rather than of some maximum because the bonus is what the bar is a picture of, and the
    -- proportion then holds at every length instead of only at the ends.
    --
    -- Only the clamp knows about anything else on screen: whatever is left of the free strip once
    -- the number in the middle has had its space, halved between the two rails.
    local rail_base = screen_w / 16
    local rail_room = (avail - gap_w - rail_gap * 2) * 0.5
    local rail_w = math.min(rail_base * (gain or 0), rail_room)

    -- Negative when the viewport centre has been squeezed past one of the side blocks, which is a
    -- window too narrow for a centre column at all.
    if avail > 0 then
        local feats_y = bottom - label_size
        -- Centred in the strip and nothing more. Unlike the rails the feats have no length to give
        -- back - the row is as wide as its own words - so the only lever is where it starts, and
        -- pinning that to the left edge would not save the row, it would just decide that the
        -- rating block is the one it runs into. Centred, an overrun costs both sides equally, and
        -- it takes an unplayably narrow window and a full hand of feats to get there at all.
        local fx = block_cx - feats_w * 0.5
        for _, feat in ipairs(feats) do
            local icon = FEAT_ICON[feat]
            if icon then
                tw.hud.icon(icon, fx, feats_y + (label_size - icon_size) * 0.5, icon_size, DIM)
            end
            fx = fx + icon_size + icon_gap
            tw.hud.text(fx, feats_y, feat, DIM, label_size)
            fx = fx + tw.hud.measure(feat, label_size) + feat_gap
        end

        -- The multiplier and its drop timer are two rows on one grid: one centre, one reserved gap
        -- for the number, one pair of inner rail edges. Both rows' rails grow outward from that gap,
        -- which is what makes them shrink *towards the middle* instead of each towards its own
        -- centre.
        if chain_text ~= nil then
            local colour = chain_colour(gain)
            local rail_h = math.max(3, base * 0.18)
            local inner_l = block_cx - gap_w * 0.5 - rail_gap
            local inner_r = block_cx + gap_w * 0.5 + rail_gap

            -- The glow carries the intensity, so a long chain lights up rather than merely changing
            -- hue - which is the part that is legible out of the corner of an eye. This one is on
            -- the bonus against its maximum rather than on the rail length, because the length is
            -- open-ended and a brightness has to have a top.
            local strength = (0.3 + 0.7 * math.min(gain / CHAIN_MAX, 1)) * fade

            local function rails(mid, width, height, weight)
                -- Shorter than it is thick is a dot, not a bar. Below that the rounding eats the
                -- whole shape and what is left reads as a rendering fault.
                if width <= height then
                    return
                end
                local c = tw.fade(colour, fade)
                local radius = height * 0.5
                local y0, y1 = mid - height * 0.5, mid + height * 0.5

                tw.hud.rect(inner_l - width, y0, inner_l, y1, c, radius)
                tw.hud.glow_rect(inner_l - width, y0, inner_l, y1, c, radius, weight)
                tw.hud.rect(inner_r, y0, inner_r + width, y1, c, radius)
                tw.hud.glow_rect(inner_r, y0, inner_r + width, y1, c, radius, weight)
            end

            local rows_h = chain_h + (timer_text ~= nil and timer_h + 4 or 0)
            local chain_y = feats_y - rows_h - 8
            local timer_y = chain_y + chain_h + 4

            rails(chain_y + chain_h * 0.5, rail_w, rail_h, strength)
            -- The number stays white at every chain length: the rails carry the colour, and two
            -- things changing colour together is one thing too many.
            tw.hud.text(block_cx - chain_w * 0.5, chain_y, chain_text, tw.fade(WHITE, fade), chain_size, "semibold")

            -- The countdown is only shown while there is a chain to lose. The collection pass runs
            -- either way, but with no chain the number is a countdown to nothing in particular, and
            -- read on its own - with no rails beside it and a multiplier of x1.0 above it - it looks
            -- like a counter of something unexplained.
            --
            -- Held, not switched: this rises to full over the same roll that grows the rails, so
            -- starting a chain brings the row up with them and dropping one takes it away with them.
            -- The first envelope key is where a chain is worth anything at all, so that is where it
            -- reaches full weight.
            local held = math.min(gain / CHAIN_FIRST, 1)
            if timer_text ~= nil and held > 0.01 then
                -- The chain's own rails scaled by the time left, so the row below is literally a
                -- picture of how much of the row above survives if nothing matches.
                rails(timer_y + timer_h * 0.5, rail_w * timer_frac, math.max(2, rail_h * 0.7), strength * 0.7)
                tw.hud.text(block_cx - timer_w * 0.5, timer_y, timer_text, tw.fade(tw.alpha(WHITE, held), fade), timer_size,
                    "semibold")
            end
        end
    end

    -- -- ---- footer, above the table ------------------------------------------------------------
    -- -- The breakdown is the part the game's own counter cannot produce: how much of a run went
    -- -- through an ability rather than straight into the grid, and how many blocks the game invented.
    -- local footer = league_name
    -- if false then
    --     -- (nothing to report while loading now that the totals come from the game)
    -- elseif all_total > 0 then
    --     footer = footer .. string.format("   %d/%d traffic  %.0f%%", got_total, all_total, 100 * got_total / all_total)
    -- end
    -- footer = footer .. string.format("   grid %d  buffer %d  erased %d", by_route.grid, by_route.buffer, by_route.erased)
    -- if powerups > 0 or ghosts > 0 then
    --     footer = footer .. string.format("   powerups %d  ghosts %d", powerups, ghosts)
    -- end

    -- local fh = select(2, tw.hud.measure(footer, label_size))
    -- tw.hud.text(tx, swatch_y - fh - 6, footer, FAINT, label_size)
end)
