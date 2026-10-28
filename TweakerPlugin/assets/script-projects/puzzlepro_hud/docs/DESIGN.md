# PuzzlePRO HUD - design notes

A run tracker that replaces several pieces of the game's own HUD rather than sitting next to
them. Everything it draws is measured, themed and positioned against the viewport; nothing is
hardcoded except padding.

The interesting parts are documented where they happen. Fourteen things are worth reading first,
because they are the reasons this is not the obvious script.

## 1. WHAT IT MEASURES - by track ROW, not by collection EVENT

The obvious tracker counts TrafficCommander::Do_CollectCar (#763) and divides by the generated
track. That over-counts, visibly: on a colour a good player clears to the last block it goes past
100%, by roughly the number of power-ups on the track.

The cause is not that the game subtracts anything. It adds a car that is not in the track.

    IfElse #985:  QuestionBoxState[CurrentTraffic] > 0 && !DumptyScoopDown?
                      -> Do_CollectQuestionBox (#986)     -- Do_CollectCar never runs
                  else
                      -> Do_CollectCar (#763)

and #986 ends with:

    Traffic.Type[CurrentTraffic] := -1
    BirthID := CurrentTraffic
    call Do_BirthCar                       -- the INNER one (#207), not the wrapper (#756)

The wrapper is where the bookkeeping lives: it advances Index_TrafficPattern to the next row,
bumps NextStatCarToBirth, writes Traffic.PrecalcedArrayNum, and range-checks against
TotalCarCount. Bypassing it births a *duplicate of whatever row the spawner used last*, into the
slot the power-up just vacated - a real, collectible car with no row of its own. That row is
still spawned again later, normally.

So each power-up costs the colour tally -1 on the power-up's own colour (its collection never
reaches Do_CollectCar) and +1 on the duplicated row's colour. The total is right; the per-colour
split is not.

Both halves are fixed by counting rows instead of events. Do_CollectCar hands us the row id for
free - it is the second thing the handler does:

    TrafficType := Traffic.Type[CurrentTraffic]
    TrafficID   := Traffic.PrecalcedArrayNum[CurrentTraffic]

A legitimately born car's id is unique; a ghost carries the id of the power-up that vacated its
slot, which we have already spent. So: hook both handlers, count each row once, and the ghosts
fall out by themselves. See count_row() in src/tally.lua.

What this deliberately does NOT do is de-duplicate the bonus prediction. Achievements::
Do_ReportBlockHit (#1088) does not de-duplicate either - it increments {colour}Hit and
TotalBlocksHit unconditionally - and those are the counters Do_CalculateFinalStats copies into
CollectedColorCounts and tests at >= 0.95 for Butter Ninja and Seeing Red. The prediction has to
count what the game will count, ghosts and all, so it reads those channels directly.

## 2. WHERE THE DENOMINATORS COME FROM - the live pattern, and WHEN to re-read it

The track is StatCollector::Stats: TrafficPattern (#57): one row per block, colour id in x, ring
in y, lane in z. Bucketed the way Achievements::Do_GetTrafficCounts buckets it (see BUCKET in src/colours.lua), that
walk *is* the game's {colour}Total - the number Do_CalculateFinalStats divides by for the 95%
Butter Ninja / Seeing Red test. So walking it is not second-guessing the game; it is computing
the same number early.

Two other sources look easier and are both wrong, which cost two rounds of this:

  * **Stats: TrafficColorCounts is not it.** Do_CountColors (#940) fills that at track generation
    - and white and wild blocks do not exist yet at that point. They are placed afterwards, by
    Do_ReseteWhiteWildBlocks (#1156), which **overwrites already-coloured rows**:

        Do_RestoreToOriginalState : every row back to Stats: Old Color / Old Lane
        Do_PlaceWhites (#1170)    : NumWhites random rows with x < 6  ->  x := 7
        Do_PlaceWilds  (#1171)    : NumWilds  random rows with x < 6  ->  x := 6.1
        Do_GeneratePowerups(#1528): QuestionBoxState per row, seeded by the song title

    So TrafficColorCounts still counts a red that has since become a white. It is too big on
    every colour and exactly zero on white - which is why the white column read "--" and why
    Seeing Red went unpredicted on tracks with few reds, where losing one to a white is the
    difference. The game gets this right because Do_GetTrafficCounts re-walks the live pattern at
    the end of the run, and Do_CalculateFinalStats then overwrites TrafficColorCounts with the
    result - which is the couple of frames of the numbers visibly correcting themselves just
    before the results screen.

  * **"Rescan when TotalCarCount changes" is not it either.** Nothing above changes the number of
    blocks. Neither does Do_CharacterAndLeagueTrafficMods, which recolours the track for Mono,
    Casual and Pro (Do_NinjaMagic, Do_TurnYellowRed, Do_TurnPurplesBlue,
    Do_LimitTo4ColorsForProMode). The count is the one thing that does not move.

The rescan is therefore driven by the events that mean "the track is now what it will be", never
by watching a number - see the hooks in src/tally.lua.

## 3. WHAT IT TAKES OVER

    XX_gui::Do_LadderMedalRequirements (#3464)  the medal dashboard
    XX_gui::Do_ShowSongName            (#2955)  the song title
    XX_gui::Timer1                     (#3938)  the chain bar
    XX_gui::Timer2                     (#3970)  the song progress bar
    XX_gui::Do_ShowScore               (#2351)  the top frame, the score, the "+N" floaters
    XX_gui::If                         (#5412)  the "Best Score" popup
    XX_gui::Do_CooperativeModeSeparateScores (#3707)  the two Double Vision scores
    XX_gui::Do_OverheadMap             (#4147)  the minimap

Every one is a pure render branch, and in each case the choice of node is the entire safety
argument:

  * #3464 - rings, cover shape, celebration and flash timers hang off it and nothing else does;
    nothing downstream reads a value it produces.
  * #2955 - writes only SongnameX / SongnameEmissive / SongNamePositionSet?, and no other group
    in the project imports any of the three.
  * #3938 / #3970 - Timer1 is the chain bar and Timer2 is the song progress bar, both under
    Do_ShrinkingBars (#3936). They are muted as two objects rather than as their parent
    Do_ShowChainBar (#398), because #398 also carries the Z - Buffer Clear (#3937) the rest of
    the GUI is drawn against.
  * #2351 - the whole strip in one cut. It hands nothing outward: LastKnownPoints (#5631) and the
    PointFloaters table (#5642) are read by the branch that writes them and by nothing else.
  * #5412 - the draw branch INSIDE Do_ShowPersonalBest, not Do_ShowPersonalBest (#5348) itself.
    Muting the parent would take a Trigger (#6721) with it that toggles Player::AutopilotOn? on
    SPACE in Freeride. Cutting at #5412 also leaves Set Value #12349 running, which is what keeps
    XX_gui::PB (#731) - the game's own `Online? ? MAX(PB_FromServer, PersonalBestScore) :
    PersonalBestScore` - fresh for us to read.
  * #3707 - two CloneScoreText objects under a gate on SpecialPurpose::useClone?, nothing else.
  * #4147 - the side view and its dot. It does write one value on the way, Highway::RingIndex :=
    Player::OffsetCurrentRing - but RingIndex is a scratch cursor that each of the seven groups
    reading Highway sets for itself first, so nothing reads what the minimap left there.

And one node that looks exactly like the others and must NEVER be muted:

    XX_gui::Do_RenderMoneyParticles (#3980)

It is not a render branch. It calls MoneyFloaterCommander::Do (#1), which is ONE loop that draws
each particle, ages it, and on the frame its life runs out calls Do_EndLifeApplyScore (#198) ->
StatCollector::Do_UpdatePoints. Muting it does not hide the flying points; it stops the game
awarding any score at all, for the rest of the run. If the blobs ever need to go, the node for it
is MoneyFloaterCommander::MoneyFloater_GiveAlpha (#4), inside the loop.

Muting is also the right tool for the song title rather than driving the Tweaker's own
hidden_song_title tweak: a script has no access to the host's tweak state, and even with access
it would be sharing a toggle with the user. A mute needs no coordination - if the tweak is on,
the game's own If (#6378) already skips the call; if it is off, the mute catches it - and it is
removed the moment the script is disabled.

## 4. THE CHAIN MULTIPLIER

Puzzle::ChainCount (#468) counts consecutive collections, capped at 100. What it is worth is not
linear in it: Do_SetMoneyMultiplier (#94) does

    ScoreBonus := 1
    if MultipleColorsMatched? : ScoreBonus += 0.3
    if ChainCount > 0         : ScoreBonus += Envelope#643(ChainCount)

and Envelope #643 is a six-key curve, 0/1/4/10/20/100 -> 0/0.5/1.5/2/2.5/3. So the displayed
multiplier runs 1.0 to 4.0, and the bonus behind it runs 0 to 3.

That curve is also the answer to "how do I put a logarithmic scale on a linear colour gradient":
the game has already linearised it. The indicator's colour walks the block palette one stop per
key, so the colour moves at the rate the bonus actually grows.

The bar length is not this script's invention either. The game's own chain bar is

    XX_gui::Timer1 (#3938) width = Inertia( (15.1041/3) * Envelope#12477(ChainCount) )

- the same six-key curve, and **linear in the bonus rather than in ChainCount**. That is the part
worth copying: a bar that grows with the bonus is the one the player has spent every previous run
learning to read at a glance. What is not copied is the constant. The game's own is a fixed world
length; a rail here is a sixteenth of the viewport per point of bonus, because a bar tuned to look
right at 1280 is a smear at QHD. The Inertia is why the game's slides rather than snaps; here that
is roll().

## 5. THE CHAIN DROP TIMER

The chain is not held for a while and then released by a timer of its own. What exists is the
match collection pass, and the chain is whatever survives it:

    Do_CollectMatchesOnTimer (#113) -> Do_ManageCollectionTimer (#455)

        MatchTimer := (HintmanEliteFreezingMatchCollection? || BlocksSlidThisFrame?)
                          ? MatchTimer
                          : MatchTimer + StartGroup::PausableTickCount

        if MatchTimer > SpecialPurpose::MatchCollectionTicks:
            MatchTimer := 0
            ...find matches...
            if any        -> score them, Do_IncrementChainCount (#1437)
            if none       -> Do_GraduallyResetChain (#1500)

So the countdown to "the chain is judged" is MatchCollectionTicks - MatchTimer, and it freezes
while blocks are sliding - which is why the number holds still through a collapse. That freeze is
also why the remaining time is read from the game every frame rather than timed here: a clock of
our own would keep running through it and promise time the player does not have.

Ticks are 25ths of a second. StartGroup::PausableTickCount is Quest3D's TickCount gated on the
pause screen (Stage/XX_Timer.cgr #0), and every second-valued counter in the game divides it by
25 - StatCollector::Timer is `Timer + PausableTickCount/25`, and FirstPersonGracePeriod, the
constant it is compared against, is 5.

What losing the pass costs is the league's business rather than the timer's - ChannelSwitch #1467
on LeagueID: 0 -> the chain is gone, 1 -> FLOOR(chain/2), 2 -> gone. Only Pro halves it.

MatchCollectionTicks is per character and per league rather than a constant: 10/10/20 by league,
overridden to 15 for Easy Ninja, 17 for Ninja Pro, 25 for Eraser Elite, 28 for Ninja Mono, 35 for
Berserker, 50 for Freeride, and to whatever a song's own tag asks for. Reading the channel rather
than assuming a number is the entire reason the bar is right for every character.

## 6. WHY EVERYTHING MOVES THROUGH ONE FUNCTION

Every number and every bar here is drawn from roll() (src/roll.lua), a keyed tween sampled through tw.ease.
Two things follow, and both are why it is not done per widget:

  * retargeting mid-flight is the normal case rather than the exception. Points arrive in bursts,
    the chain can jump two tiers in one pass, and a tween that has to finish before it accepts a
    new target falls visibly behind the game. roll() restarts from wherever the value has got to.
  * the chain's colour, its rail length and its printed multiplier are all functions of ONE
    tweened bonus, so they cannot disagree. Tweening the three separately is three ways for the
    colour to arrive before the bar.

The drop timer is deliberately NOT tweened. It is a countdown; smoothing it would mean showing a
number that is not the time left.

## 7. THE SKILL RATING IS THE GAME'S OWN NUMBER

The game never shows it. It computes it at the end of a run and puts it on the wire, so a HUD that
gets it wrong is wrong invisibly - the player just concludes they had a bad run. Every term below
is therefore taken from the graph rather than reasoned about.

    Achievements::Do_CalcSkillRating (#1189), inside Do_FinalizeAndStringEncodeExtendedRideStats
    (#1118), which Do_CalculateFinalStats calls:

        SkillRating := MAX(1, ROUND( PointsWithGridBonus / GoldRequirement * 100
                                                         * (LeagueID + 1) ))

    StatCollector::Do_FindAddBonusPoints (#797):

        BonusPoints           += Points * <scaler>   per feat earned
        BonusPoints_CleanOnly += Points * GridBonusMultiplyer   if NumTilesInGrid == 0
        PointsWithGridBonus    = ROUND(Points) + ROUND(BonusPoints) + ROUND(BonusPoints_CleanOnly)

Three things about that are worth stating because each was a candidate for the figure being wrong
and each turned out not to be:

  * **Every scaler is over the raw Points**, not compounded. So the whole thing is
    `Points * (1 + sum of scalers)` and a script may add them up.
  * **GoldRequirement is not a constant of the song.** XX_StartHere::Do_CalcMedalRequirements
    (#4267) sets it to `TotalCarCount * {10, 30, 35}[LeagueID]`. Read the channel.
  * **LeagueID is a property of the character**, written by XX_WindowState::Do_SetCharacter (#307)
    as 0, 1 or 2 - not of the difficulty, which is ChosenDifficulty and drives a different set of
    medal requirements entirely. `LeagueID + 1` is the 1/2/3 multiplier.

What WAS wrong was the last clause of the middle line. `NumTilesInGrid == 0` is tested **at the
moment the game scores**, so the Clean Finish bonus belongs to the player for as long as the board
is clear - it is not an optimistic extra. Showing it only as a parenthesised maybe under-reported
the headline by a fifth for most of a good puzzle run.

## 8. THE POINTS ARRIVE BEFORE THEY ARE COUNTED - and that is the game's own clock

One collected match takes three separate visuals in the stock HUD, and they happen in this order:

    Puzzle #1336   inDisplayedMoney := PointsThisTile
                   inLaunchColor    := Puzzle_State.MatchedColors[...]
                   call MoneyFloaterCommander::Do_LaunchParticle (#27)
                        -> a row in MoneyParticles, plus the cash-register sound
    ~~~ the particle flies, at SpeedModifier = Min + RAND*Max, drawn RANDOM PER PARTICLE ~~~
    #198           SpentLifePercentage >= 1 -> Do_EndLifeApplyScore
                        -> StatCollector::Do_UpdatePoints -> Points += delta
    next frame     XX_gui::Do_LaunchScoreFloaters (#3086) notices PointsByColor moved
                        -> spawns the "+N" text
    every frame    TotalResourcesText (#2480) = ROUND(Inertia(Points))

So the game's "+N" really does appear after the score has already grown. Inverting that is the
point of this widget - but the inversion does not need a timer of its own, because **the game is
already holding the score back for the length of the particle's flight**. Drawing the number at
#27 and rolling the total towards Points gives exactly the requested order, on the game's clock.

**A particle is launched per matched TILE, not per match.** Do_LaunchMoneyParticles (#95) loops
over every block in the collection, so a seven-block match is seven launches carrying the same
colour and the same PointsThisTile. Announcing each one separately printed seven identical numbers
in the same place. The game has the same problem and solves it by building its text floaters from
a once-a-frame diff of PointsByColor, which merges them per colour; this merges them per colour at
the point of launch, which comes to the same set of numbers.

A merged entry is then held until the LAST of its particles has landed, because that is when the
game has finished adding its points. Do_EndLifeApplyScore runs inside the particle loop, so
Index_Particles is on the row that landed, and one read matches the landing to the entry waiting
for it.

Not every point goes through a particle. Do_UpdatePoints is called directly by the ninja ram
bonus (Puzzle::Do_HitColor #1228), the penalties (#1205), BusyBee, the question box and boss
damage. Those are caught by hooking Do_UpdatePoints itself and asking "was this one already
announced by a particle landing?" - Do_EndLifeApplyScore runs immediately before the call it
makes, so a counter bumped there and consumed here separates the two exactly.

Two things about that hook are deliberate:

  * **the delta is Points-now minus Points-then, not UpdatePoints_Delta.** The parameter channel
    would need the group-parameter push to work the way it appears to; the difference needs
    nothing but two reads, and it also catches the `Points := MAX(0, Points + Delt)` clamp, which
    is the amount the player's score actually moved.
  * **the colour can be stale.** UpdatePoints_Color (#155) is written by three sites and
    Do_UpdatePoints is called from ten, so some events inherit the previous event's colour. The
    game has the same bug - its PointsByColor table is indexed by that same channel - so matching
    it is matching the game, but an unrecognised id is drawn neutral rather than guessed at.

## 9. THE TOP STRIP IS A QUEUE ON A BACKDROP, AND BOTH ARE THERE FOR THE SAME REASON

Three things about the top of the screen that only showed up in a real run.

  * **It is measured from the viewport, not from the safe area.** tw.hud.safe() exists to keep a
    widget clear of the overlay's own top chrome, and obeying it here put the score a tenth of the
    way down the screen - far enough in to sit over the track and cover oncoming traffic. The
    chrome it was avoiding lives in the corners; this block is centred and narrow, so it passes.

  * **It has a backdrop, because white text on a bright skybox is not text.** The game puts a
    solid bar behind its score for exactly this reason. This is the same idea without the bar:
    flat top and bottom, full strength across the content, and a horizontal falloff to nothing on
    both sides, so it reads as the screen getting darker rather than as a panel sitting on it. The
    first stop is the end of the content, so nothing that is actually behind text is in the
    falloff. Symmetric on purpose: the queue extends left only, and a backdrop that grew on one
    side to follow it would change shape on every match.

  * **The numbers are a queue, not a heap.** Each entry sits behind the one in front of it by its
    own width plus a gap, and appears one step later. The slots are recomputed every frame from
    whoever is still on screen, so when the head leaves - which is when the total takes its points
    - everything behind slides up into the space. That sliding is why the position is no longer
    read from the particle: per-particle progress looked right with one number in the air and put
    every colour of a multicolour collection in the same place at the same time.

## 10. THE SCORE ON SCREEN IS THE PROJECTED FINAL SCORE, NOT StatCollector::Points

The personal best is not a raw score. LocalScores::Do_WriteScore stores

    PersonalBest.score[index] := StatCollector::PointsWithGridBonus

and the server copy is the same figure uploaded, so `best` is the final score WITH every bonus.
Showing raw Points beside it compared two different quantities: raw runs some 20-50 % under the
figure the record is made of - Clean Finish alone is 25 % - so the gap read as "you are having a
bad run" for the whole of a good one.

So the headline is the same projection the skill rating is built from (§7): raw Points times one
plus the scalers earned so far, with Clean Finish in it while the board is clear and in the
bracketed figure when it is not. One computation, three consumers - the rating, the score, and the
multiplier below.

**The points in flight are scaled by that same multiplier**, and the reason this is exact rather
than approximate is worth stating, because it looks like it should not be. Do_FindAddBonusPoints
adds each feat as `Points * scaler` over the RAW points - never compounded, never against a
running subtotal - so the whole projection is `Points * (1 + SUM scalers)`. The factor is common
to every point in the run, and a common factor distributes:

    SUM_i ( d_i * (1 + B) )  ==  ( SUM_i d_i ) * (1 + B)

The numbers flying in therefore still sum to the total, exactly, and the widget's contract - the
number lands and the total takes it - survives. It would not survive showing raw amounts against a
projected total: every match would move the total by half again what its number said.

The one thing that does not come from a floater is a change in B itself: earning Match21 or
clearing the board steps the headline with nothing flying in. That is a real event rather than an
error, and the bracketed figure swapping sides is what explains it on screen.

## 11. THE MINIMAP IS THE GAME'S OWN, REDRAWN - AND COLOURED BY WHAT IS ON THE TRACK

The game's minimap is the track seen from the side. XX_gui::Do_OverheadMap (#4147) ->
Do_RenderTrackSideView (#4808) bakes a 512x256 texture once per track from
Routemap_TrackPixels (a pixel is lit where the profile passes through its column), and puts a dot at

    x = OffsetCurrentRing / NumRings
    y = (y(Highway::Rings: Pos[ring]) - LowestRingYpos) / (HighestRingYpos - LowestRingYpos)

So the profile is the height of every ring of Highway's spine against its number, and that is what
src/track.lua samples - the same channels, read directly:

    Highway::NumRings, Rings: Pos (y), LowestRingYpos / HighestRingYpos, Rings: SongTime
    Player::OffsetCurrentRing + OffsetCurrentRingInterpolation

Three things a probe run settled before any of it was drawn:

  * **The rings exist at the first run-start hook.** NumRings, Rings: Pos and Rings: SongTime are
    filled by Do_ResetStats already, so the profile is walked on the same three events as the colour
    totals (§2) and never waits on a number.
  * **The blocks are on the same axis.** TrafficPattern.y is a ring number: Do_BirthCar sets
    TrafficCommander::RingToDieAt := y (#713), the ring at which the block reaches the player. In real
    runs every y was a whole number inside [0, NumRings). So the walk §2 already makes over the track
    files each block under the stretch it is on without a single extra read - which is what makes a
    coloured map cheap.
  * **Rings: SongTime is seconds** from the start of the song: 271.16 at the last ring of a 4:31 song.
    "Time left" is the last ring's time minus the player's.

256 samples, not every ring. A track is 8 to 13 thousand rings - about fifty to the second - and its
height is a running sum of slopes, so it is smooth; a line a few hundred pixels long cannot show more
than a few hundred points anyway, and the game itself only keeps 512 columns.

### The colour

The map is cut into stops of `batch` rings (about two seconds). A stop's colour is its blocks' colours,
each weighted by how many there are times what one block of that colour is worth, with every weight
raised to a steep power first, so that the colour that dominates the stretch takes the stop. The line
between stops is a gradient (tw.hud.gradient_polyline), so the map changes colour where the track does,
smoothly. Stop size, softness and the five weights are the player's.

The first version counted blocks one for one, softened with a gentle power over a window of five stops,
and the map came out wrong in a way that was obvious on the first track. Purple is the commonest block on
any track - it is the quiet one - so it outvoted everything; and a stop averaged over four colours is, in
RGB, a colour nobody put on the track: yellow and red with a little blue in them came out light green. So
the default weights are the game's own block values, StatCollector ChannelSwitch #128 (`cgrpy query switch
StatCollector 128`): 1, 2, 3, 5, 8 from purple to red - a red block is worth eight purple ones, and a
stretch of them should look like it. The power runs from 8 (hard) to 2 (soft), and at most two stops either
side are blended in.

White and wild blocks colour nothing. A stop without a coloured block takes its neighbours' colour
instead of going grey. A track that carries one colour only is that colour end to end - which covers
Mono, every Ninja, Freeride and the songs whose tags force them, without a table of modes: it is the
track, not the character, that says so, exactly as the table's columns are chosen (§2).

The scan keeps its counts per 16 rings, finer than any stop, so moving the stop size regroups counts
instead of walking the track again.

### The line

Two lines split at the player, the stretch already ridden at a third of its strength. That is the song
progress bar back: XX_gui::Timer2 (#3970) has been muted since the chain bar was (§3), and until the map
nothing showed how far through the song the player was. The split point is computed, not snapped to the
nearest sample, so the bright part starts under the dot.

## 12. THE SONG BADGE SHOWS THE NAME, NOT THE STRING

The game's title is StartGroup::CurrentSongTitle and ::CurrentSongArtist, drawn through
StartGroup::Texter_Unicode by Do_ShowSongName (#2955, muted, §3). The handle hands them over as UTF-8:
the channel says whether it holds a wide string, and a title that came off a CD or out of a tagged file
does. Whether every character of it then has a glyph is the overlay's font's business - Latin, Cyrillic
and Greek do, CJK does not yet.

The string is not the name, in two ways:

  * **Tags are part of the title.** Authors put rules in a file's title - "[as-monoonly]" makes the game
    play it as Mono - and the game appends its own when it loads the song: XX_StartHere #6835...
    `CurrentSongTitle := Unicode_Merge(CurrentSongTitle, " [as-steep]")`, the same for [as-portal]. A
    real title arrived as "Drive my Life [as-steep] [as-prows1] [as-monoonly] [as-allgrey]". They are cut
    (`%s*%[as%-[^%]]*%]`): they are rules rather than the name, and what they do to the run is on screen
    already - the colour table's columns, the feats.
  * **"Unknown Artist" is not an artist.** It is what VisMusic writes (#252) when a file has no tag. The
    badge drops the artist line rather than print the absence of one.

Both strings are read on the run-start events and twice a second while the HUD is up - they change once
a song, and every read makes a new Lua string.

The badge is as wide as its text, up to the point where the chain block would reach with the chain at its
maximum (chain_block.max_half_width). The other way round - letting the title push the chain - would
shorten the one bar on screen the player reads by its length, by an amount that depends on the song. A
title that does not fit scrolls: it rests at its start, slides along to show the rest and comes round to
the start without a jump, a second copy following the first. It is drawn glyph by glyph with each glyph
faded towards the edges, because a soft edge needs a per-glyph alpha in any case and a clip rectangle
would only give a hard one; the glyphs and their offsets are measured once per title, so a scrolling
frame allocates nothing. The ellipsis is the other choice, for whoever finds movement at the edge of the
screen distracting.

## 13. THE BOTTOM IS ONE BAND

The song badge, the chain block and the rating sit in one strip across the bottom, and its height is the
chain block's at its fullest - the multiplier, the drop timer and the feats - computed from the font sizes
rather than from what the chain block shows this frame. That distinction is the whole point: the timer row
comes and goes with the chain, and a band that followed it would move the badge and the rating every time
a chain started or dropped. The badge fills the band; the rating is centred in it; the chain block hangs
from its top.

The chain block is centred on the viewport and its room is the nearer of the two sides mirrored (§4,
draw/chain_block.lua). The badge's right edge is the left side now; before the badge it was the colour
table, which has moved up into the stats panel with the minimap.

## 14. THE SETTINGS ARE THE PLAYER'S, AND THE FILE IS NOT TRUSTED

The tab in the overlay menu (src/settings.lua) holds what is a matter of taste rather than of fact: how
long a stretch of track gets one colour on the minimap, how softly the colours blend, how much each block
colour counts, whether the time left is shown, and what a title too long for its badge does. Everything
the HUD measures stays out of it - there is no setting that could make a number on screen disagree with
the game.

The values live in ScriptData\puzzlepro_hud\settings.json, read once when the script loads and written
when the player finishes a change - a slider when it is let go, so dragging one previews live on the map
without writing the file every frame. Every field of the file is checked on the way in and falls back to
its default on its own: a file from an older version, or one edited by hand, costs the player at most
that one setting, never the HUD.

The colour weights default to the game's block values (§11), and a button puts them back: those are the
one set of numbers that has a reason behind it rather than a preference.
