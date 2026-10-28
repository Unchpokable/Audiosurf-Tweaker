-- Every channel handle the HUD reads, created once, here, in one place. Each resolves itself lazily.
-- Creating one inside on_frame would rescan the whole group every frame forever - see
-- Docs/scripting/channels.md.
--
-- Returned as one table so the modules that read them name the channel they mean; each module takes
-- local aliases for the handful it uses.

local colours = require("colours")
local YELLOW, RED = colours.YELLOW, colours.RED

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

-- The chain drop timer (DESIGN.md §5). Both names are unique in their own groups, so neither needs an
-- index - and neither should get one: MatchCollectionTicks is written from a dozen places and an
-- index would make the read more fragile without making it more precise.
local match_timer = tw.float_ch("Puzzle", "MatchTimer")
local match_ticks = tw.float_ch("SpecialPurpose", "MatchCollectionTicks")

-- Score, personal best and the points in flight (DESIGN.md §8).
--
-- PB (#731) is XX_gui's own, already-resolved personal best - Online? ? MAX(PB_FromServer,
-- PersonalBestScore) : PersonalBestScore - and it stays fresh because the mute cuts below the Set
-- Value that writes it. Reading it beats reassembling the same expression from three StartGroup
-- channels, which is three more chances to pick the wrong branch.
local personal_best = tw.float_ch("XX_gui", "PB")
local points_l = tw.float_ch("StatCollector", "Points_L")
local points_r = tw.float_ch("StatCollector", "Points_R")
local use_clone = tw.float_ch("SpecialPurpose", "useClone?")

-- By index: StatCollector has two channels called UpdatePoints_Color (#155 and #1753), and #155 is
-- the group parameter Do_UpdatePoints actually reads.
local update_colour = tw.float_ch("StatCollector", 155)

-- The launch parameters, and NOT the two channels that look like them. MoneyFloaterCommander
-- declares Launch_DisplayedMoney (#7) and Launch_Color (#58) as its group parameters, and every
-- caller binds them - but the body of Do_LaunchParticle reads these two instead. #7 and #58 are
-- vestigial: nothing in the group links them to anything, so they hold whatever they were born
-- with.
local launch_money = tw.float_ch("MoneyFloaterCommander", "inDisplayedMoney")
local launch_colour = tw.float_ch("MoneyFloaterCommander", "inLaunchColor")

-- Which particle row is being dealt with right now. It is the cursor of the MoneyParticles table,
-- and both places this script cares about run with it already pointing at the row they mean: a
-- launch has just filled it, and a landing is the loop iteration for it. That makes one number
-- enough to match a landing to the queue entry that is waiting for it - no table reads needed.
local particle_at = tw.float_ch("MoneyFloaterCommander", "Index_Particles")

-- The track's shape and where the player is on it, for the minimap (DESIGN.md §11). The game's own
-- minimap reads exactly these (XX_gui::Do_RenderTrackSideView #4808); every name is unique in its group.
--
-- Rings: Pos is the track's spine, one ring per row, and its y is the height the side view plots. The
-- rows of the track (TrafficPattern.y) are numbers on this same axis, so blocks and profile need no
-- conversion between them. Rings: SongTime is seconds from the start of the song.
local num_rings = tw.float_ch("Highway", "NumRings")
local lowest_ring = tw.float_ch("Highway", "LowestRingYpos")
local highest_ring = tw.float_ch("Highway", "HighestRingYpos")
local ring_pos = tw.array_vec("Highway", "Rings: Pos", "RingIndex")
local ring_time = tw.array("Highway", "Rings: SongTime", "RingIndex")
-- A whole ring number and the fraction of the way to the next one: the two together let the dot glide
-- instead of stepping ring by ring.
local player_ring = tw.float_ch("Player", "OffsetCurrentRing")
local player_ring_frac = tw.float_ch("Player", "OffsetCurrentRingInterpolation")

-- What is playing (DESIGN.md §12): the two text channels the game's own song title draws, through
-- StartGroup::Texter_Unicode. Wide in the game for anything that came off a CD or a tagged file; the
-- handle hands them over as UTF-8.
local song_title = tw.string_ch("StartGroup", "CurrentSongTitle")
local song_artist = tw.string_ch("StartGroup", "CurrentSongArtist")

return {
    startup_state = startup_state,
    gameplay_state = gameplay_state,
    paused = paused,
    traffic_pattern = traffic_pattern,
    car_count = car_count,
    points = points,
    best_match = best_match,
    tiles_left = tiles_left,
    bronze_at = bronze_at,
    silver_at = silver_at,
    gold_at = gold_at,
    league_id = league_id,
    scaler = scaler,
    hits = hits,
    is_ninja = is_ninja,
    is_freeride = is_freeride,
    is_mech = is_mech,
    is_portal = is_portal,
    perfect_ninja = perfect_ninja,
    prevent_mono = prevent_mono,
    type_ch = type_ch,
    traffic_id = traffic_id,
    current_traffic = current_traffic,
    precalc_row = precalc_row,
    scooping = scooping,
    erasing = erasing,
    chain_count = chain_count,
    chain_curve = chain_curve,
    match_timer = match_timer,
    match_ticks = match_ticks,
    personal_best = personal_best,
    points_l = points_l,
    points_r = points_r,
    use_clone = use_clone,
    update_colour = update_colour,
    launch_money = launch_money,
    launch_colour = launch_colour,
    particle_at = particle_at,
    num_rings = num_rings,
    lowest_ring = lowest_ring,
    highest_ring = highest_ring,
    ring_pos = ring_pos,
    ring_time = ring_time,
    player_ring = player_ring,
    player_ring_frac = player_ring_frac,
    song_title = song_title,
    song_artist = song_artist,
}
