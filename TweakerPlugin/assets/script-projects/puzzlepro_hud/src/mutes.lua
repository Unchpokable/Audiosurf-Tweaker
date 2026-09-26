-- ---------------------------------------------------------------------------------------------
-- What the game stops drawing for as long as this script is loaded. Each handle is kept so it can
-- be put back with :off() without a reload.
-- ---------------------------------------------------------------------------------------------
local muted = {
    medals = tw.mute("XX_gui", "Do_LadderMedalRequirements"),
    song_name = tw.mute("XX_gui", "Do_ShowSongName"),
    -- By index on purpose: "Timer1"/"Timer2" are unique in XX_gui but say nothing, and the two are
    -- one place apart in a handler whose name mentions neither. See DESIGN.md §3.
    chain_bar = tw.mute("XX_gui", 3938),
    song_progress = tw.mute("XX_gui", 3970),
    -- The top strip in one cut: frame, score and the game's own "+N" text floaters.
    score = tw.mute("XX_gui", 2351),
    -- The DRAW branch of Do_ShowPersonalBest, not Do_ShowPersonalBest - #5348 carries the Freeride
    -- autopilot trigger, and cutting here also leaves XX_gui::PB being written. DESIGN.md §3.
    best_popup = tw.mute("XX_gui", 5412),
    coop_scores = tw.mute("XX_gui", 3707),
}

return muted
