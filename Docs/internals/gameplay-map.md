# Gameplay map

[← back to the index](../internals.md)

Where the game keeps what a script usually wants: the run's statistics, the score, medals and rating, the
colours, the board, the chain, the HUD. [How Audiosurf is built](../scripting/game-model.md) has the short
starting map; this is the long one, with indices and with the traps that cost the most time. What each thing
means to the player is in [Playing Audiosurf](playing-audiosurf.md).

Indices are positions in the Steam release's groups. A name that is unique in its group is safe to use as a name;
where it is not, the index is given and the page says so. Check either with `cgrpy query` before building on it.

Most of the traps on this page were found the hard way by **PuzzlePRO HUD**, the first script that read a large part
of the game's state and replaced half of its HUD. Its
[design notes](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/TweakerPlugin/assets/script-projects/puzzlepro_hud/docs/DESIGN.md)
tell each one as it happened — what the obvious script did, what the player saw, what the graph said — and are worth
reading before writing a tracker, a score display or a HUD replacement. The same notes open the shipped
`puzzlepro_hud.lua` in the game's `engine\TweakerStuff\Scripts\` folder.

## `Scores/StatCollector` — the run

The statistics of the run, the score and the end-of-run calculation. Every other group reports into it through its
four parameters ([graph format](graph-format.md#imports-how-groups-call-each-other)).

### The run's counters

One `Set Value` (#11, in `Do_ResetScores`) zeroes all twenty per-run counters — the canonical list:

| Channel | # | Written by |
|---|---|---|
| `Timer` | 13 | `Do` (#0), every frame: seconds into the run |
| `Points` | 152 | `Do_UpdatePoints`: `MAX(0, Points + delta)` |
| `LargestMatch` | 12 | `Do_ReportMatch`: `MAX(size, LargestMatch)` |
| `Points_L`, `Points_R` | 651, 652 | `Do_UpdatePoints` — the two halves of a Double Vision score |
| `NumCombos` | 180 | `Do_AddCombo` — matches of more than one colour (Sync Bonus) |
| `NumChainReactions` | 183 | `Do_AddChainReaction` |
| `TimeOnShoulder` | 9 | `Do_ShoulderTimer` |
| `TimeInFirstPerson` | 10 | `Do_FirstPersonTimer` |
| `HitsWithMouseControl`, `HitsWithoutMouseControl` | 1226, 1225 | `Do_ReportCarCollected`, by `Player::CurrentInputMethod` |
| `YellowsPassed`, `RedsPassed` | 1251, 1252 | `Do_ReportCarPassed` |
| `NonRedNonYellows_Hit` | 1263 | `Do_ReportCarCollected` |
| `Scored_A_21_Red_Earned?` | 1213 | `Do_ReportMatch`, more than 20 reds in one match |
| `FreeRideCarsHit` | 1737 | `Do_ReportFreeRideCollision` |
| `EarnedRawGold?` | 1764 | `Do_CalculateMedalEarned` |
| `NumOverfills` | 1249 | **nobody** — dead. The real counter is `Overfills` (#340) |

`Overfills`, `TotalCollisions` and `PerfectNinjaRun?` (Stealth still possible) are reset alongside, in
`Do_ResetSimpleStats`.

### Three different "starts"

| Handler | # | Fires | Means |
|---|---|---|---|
| `Do_ResetStats` | 1 | once per song | the **track is generated**. Not on a restart. |
| `Do_ReseteWhiteWildBlocks` | 1156 | every entry into gameplay | whites, wilds and power-ups are placed over the track (typo the game's) |
| `Do_ResetSimpleStats` | 345 | every entry into gameplay, restart included | the **run starts**: score, timer, counters to zero |

A restart from the pause menu sets `StartupState` to `State_Resetter`, which runs `XX_StartHere::Do_ReturnToStart`:
whites and wilds first, then `Do_ResetSimpleStats` — and never `Do_ResetStats`. A tracker that listens only to
`Do_ResetStats` keeps counting into the old run after a restart.

### The score

**`Do_UpdatePoints` (#153) is the only way the score changes.** Ten call sites in eight groups — matches, penalties,
Ironmode, power-ups, the score particles. Hook it and you see every change:

```
Do_UpdatePoints(UpdatePoints_Delta = d, UpdatePoints_Color = c):
    Points := MAX(0, Points + d)                 the applied delta can be smaller than d
    PointsByColor.Points[c] += d
    Points_L / Points_R by IncomingPointsAreSharedMatch? / IncomingPointsAreRight?
```

`UpdatePoints_Color` (#155) goes stale: only some callers write it, and values outside the palette appear (11 for
Mono's hit bonus, 14 for special purpose). The game's own `PointsByColor` has the same defect.

### Mono scores in two steps

Mono has the board, matches and the chain like Puzzle — every coloured block matches every other — and **two**
sources of points:

```
Puzzle::Do_NinjaHitBonus (#1224), on every collected block, if Ninja? && !PreventMonoStealthAndCumulativePoints?:
    stone (grey) -> Do_HitStone (#1226):  SpecialPurpose::Do_ResetNinjaBonusPoints   NinjaBonusPointsOnCarHit := 1
                                          Ironmode: also Do_RemoveOneColorFromBuffer
    coloured     -> Do_HitColor (#1228):  Do_UpdatePoints(UpdatePoints_Delta = NinjaBonusPointsOnCarHit #987, Color = 11)
                                          SpecialPurpose::Do_IncrementNinjaBonusPoints (#989):
                                              NinjaBonusPointsOnCarHit := MIN(cap, NinjaBonusPointsOnCarHit + step)
```

| League | step (`SpecialPurpose` ChannelSwitch #1155) | cap (ChannelSwitch #1117) |
|---|---|---|
| Casual | 1 | 66 |
| Pro | 4 | 200 |
| Elite | `NinjaBonusDeltaPerHit` (#993) = 4 | 200 |

Then the **matches**, through the ordinary collection pass and chain (below), at a flat value per block:
`Do_GenerateColorPointValues` (#119) fills `Stats: ColorPointValues` through ChannelSwitch #757 on `Ninja?`, and for
Mono every colour is `SpecialPurpose::NinjaBlockValue` (#1232) = **4** — a song tag can set it from 0 to 20 (#1294).
`MatchCollectionTicks` has Mono values of its own (15 / 17 / 28). A script showing "the chain does not matter in Mono"
is wrong: only the per-block payment ignores it.

### The end of the run: `Do_CalculateFinalStats` (#103)

Called by `AwardFare` (the results screen). Bonuses, each one adding a line to `Feat String`, all computed **from raw
`Points`** and never compounded:

| Bonus | Condition | Adds | Scaler channel |
|---|---|---|---|
| Clean Finish | `Puzzle::Fetch_NumberBlocksInPlay == 0` **at the moment of scoring** | `Points × 0.25` | `GridBonusMultiplyer` #163 |
| Stealth | `Ninja? && PerfectNinjaRun? && !PreventMono…` | `× 0.33` | `PerfectNinjaMultiplier` #747 |
| Match 21 / 11 / 7 | `LargestMatch` > 20 / > 10 / > 6, highest only | `× 0.21 / 0.11 / 0.07` | #819 / #963 / #954 |
| Butter Ninja | `!Ninja?` and ≥ 95 % of yellows collected | `× 0.05` | `YellowNinjaBonusPoints` #979 |
| Seeing Red | `!Ninja?` and ≥ 95 % of reds collected | `× 0.05` | `RedNinja Bonus Scaler` #989 |

```
PointsWithGridBonus = Points + BonusPoints + BonusPoints_CleanFinishOnly  =  Points × (1 + Σ scalers earned)
HighestMedalEarned  = 0..3 against Bronze/Silver/GoldRequirement, compared with PointsWithGridBonus
```

Because the final score is **linear in `Points`**, the part of the final score a single match is worth is its raw
delta times the same `1 + Σ`; the parts add up to the whole exactly. Clean Finish counts whenever the board is empty
*right now*, so a live prediction should include it while the board is empty, not treat it as hypothetical.

Read the scalers from their channels rather than writing them in.

### Medals

```
StartGroup::BronzeRequirement (#404) := StatCollector::TotalCarCount × {league factor}
StartGroup::SilverRequirement (#405)    (Do_CalcMedalRequirements, #4267, at the start of the run)
StartGroup::GoldRequirement   (#406)    gold: TotalCarCount × {10, 30, 35}[LeagueID]
```

That is the usual source, but not the only writer: `Do_SetMedalRequirements` (#407) sets the thresholds from
`SetConstants::Medals: Easy/Normal/HardGold` by `ChosenDifficulty`, and `Intros/MedalRequirements` sets them for the
Portal challenges. Which one wins when has not been mapped — **read the channels, do not recompute them**
(`cgrpy query writers StartGroup GoldRequirement` lists all five).

The in-run medal display compares **`Points`**, not the final score, so it understates progress until bonuses are
paid; the medal at the end goes by `PointsWithGridBonus`. The display hangs off one node,
`XX_gui::Do_LadderMedalRequirements` (#3464), and is pure drawing: nothing reads what it computes — it is safe to
`tw.mute`. `Do_CalcMedalRequirements` is not: it writes the thresholds.

### Skill Rating

`Support/Achievements::Do_CalcSkillRating` (#1189), at the end of the run:

```
SkillRating (#1096) := MAX(1, ROUND(PointsWithGridBonus / GoldRequirement × 100 × (LeagueID + 1)))
```

It is sent to the server and never shown. None of its three inputs is the constant it looks like:
`PointsWithGridBonus` is the bonus-applied score (above); `GoldRequirement` is per run, from `TotalCarCount`;
`LeagueID` is a property of the **character** (set per character by `Stage/XX_WindowState::Do_SetCharacter`), not of a
difficulty. A gold medal in Elite is exactly 300.

### Personal best

`XX_gui::PB` (#731) = the larger of the local and the server record, written every frame by `Do_ShowPersonalBest`
(#5348). **It is a final score** — `LocalScores::Do_WriteScore` stores `PointsWithGridBonus` — so compare it with
the projection `Points × (1 + Σ)`, never with raw `Points` (20–50 % short over a good run).

## Traffic: the blocks and their colours

### Colour numbers

Everywhere a colour is a number (`ColorID`), the meaning is the same:

| ID | | ID | |
|---|---|---|---|
| 0 | purple | 6 | wild (placed as 6.1) |
| 1 | blue | 7 | **stone**: the white block of Puzzle, the grey block of Mono |
| 2 | green | 8 | bomb |
| 3 | yellow | 9 | black |
| 4 | red | 10 | `white` — something else than the Puzzle white |
| 5 | red again (a duplicate case everywhere) | | |

### How a run's traffic is made

The generated track is `StatCollector::Stats: TrafficPattern` (#57, an `Array Vector`, cursor
`Index_TrafficPattern` #58): one row per block, `ColorID` in `x`, the ring (position along the track) in `y`. The
number of blocks is `StatCollector::TotalCarCount`.

1. `Do_ResetStats` generates it; `Do_CharacterAndLeagueTrafficMods` (#904) then folds the palette by mode — this
   is what "three colours in Casual, four in Pro" is:

   | Condition | Rewrite | Colours left |
   |---|---|---|
   | `Ninja?` or `Freeride?` | `Do_NinjaMagic`: every row becomes yellow (3) or stone, by a league threshold | 2 |
   | `ThinTraffic?` or `Ninja?` | thinning (below), and outside Mono green → blue, purple → blue | — |
   | `LeagueID == 1` | green → blue | 4 |
   | `isMechMode` | yellow → red, blue → green | — |
   | `PortalMode?` | purple → blue, yellow and red → green | — |

2. **Thinning** removes blocks that come too close together by zeroing the whole row to `(0, 0, 0)`. The row stays.
   Skip it — and check all three components, because a real purple block has a ring.
3. `Do_ReseteWhiteWildBlocks`, on every run, **recolours** existing blocks: some become stones (`x := 7`, "whites"),
   some wilds (`x := 6.1`), and power-ups are assigned (`Stats: QuestionBoxState`). Only rows with `x < 6` are
   picked, so a red that became white leaves the red count. Whites move between runs (plain random); power-ups do
   not (seeded by the song title). The number of blocks never changes — `TotalCarCount` is no signal.

### Counting blocks per colour — and why not `Stats: TrafficColorCounts`

The game's own count is `Achievements::Do_GetTrafficCounts` (#1236): it walks `Stats: TrafficPattern` and adds one
per row into `PurpleTotal` … `WhiteTotal` (IDs 4 and 5 go to red, **6 and 7 to white**, 8 and above nowhere).
Re-walk the live pattern the same way, on `Do_ResetStats`, `Do_ReseteWhiteWildBlocks` and `Do_ResetSimpleStats`,
and your numbers match the game's by construction.

`Stats: TrafficColorCounts` (#3) looks like the answer and is not: it is counted at generation, **before** whites and
wilds exist (too high per colour, zero for white), raw-indexed, and overwritten with the right numbers only in the
final scoring pass. During a run it is stale; it never contains whites.

The counters of **hits** do not agree with the totals: `Achievements::Do_ReportBlockHit` sends IDs 0–4 to
`PurplesHit` … `RedsHit`, **5 to `WildsUsed`** and 6 to `WhitesHit`, and nothing for 7. So on a track with ID-5 blocks
`RedsHit / RedTotal` cannot reach 0.95, and Seeing Red is never given. That is the game's defect; a predictor should
read the same two channels and inherit it.

### Collecting a block

**`TrafficCommander::Do_CollectCar` (#763) is the one funnel for "the car touched a block of traffic".** It reads the
colour once and branches three ways:

```
TrafficType := Traffic.Type[CurrentTraffic]                 #900 - the name is NOT unique (#631 is another)
TrafficID   := Traffic.PrecalcedArrayNum[CurrentTraffic]    #1312 - the row of the track
Traffic.State[CurrentTraffic] := 0                          off the road
if SpecialPurpose::DumptyScoopDown?   -> Pointman's stack   (Dumpty_AddOccupant)
elif SpecialPurpose::ShatterStorming? -> Eraser erases      (EraserEffect::Do_LaunchErasers)
else                                  -> onto the board     (Puzzle::Do_ResovleLaneCrash, typo the game's)
```

A power-up block does **not** go through it: `TrafficCommander` branches to `Do_CollectQuestionBox` (#986) first.
Hook both. That handler then re-births a block through the inner `Do_BirthCar` (#207), skipping the bookkeeping: a
real, collectable **ghost** block without a row of its own in the track. Count collected blocks **per track row**
(`TrafficID`), each row once, and ghosts drop out by themselves.

The ghost is a duplicate of whatever row the spawner used last, born into the power-up's slot; that row still
spawns again later, normally. So a tracker that counts `Do_CollectCar` events alone is wrong per colour in both
directions — one short on the power-up's colour (its collection never reaches `Do_CollectCar`), one over on the
duplicated row's — and goes past 100 % on a colour cleared to the last block, by about the number of power-ups.
The total looks right, which is why it survives testing.

`Do_CollectQuestionBox` does not write `TrafficID`. Its row is `Traffic: PrecalcedArrayNum` at the power-up's slot,
`CurrentTraffic` (a `tw.array` over those two), still valid in an `"after"` hook: the inner `Do_BirthCar` does not
touch it, and `TrafficType` (#900) still holds the power-up's colour.

### The trap of `CollectedColorCounts`

`Stats: CollectedColorCounts` and the `…Hit` counters are incremented for **every block that lands on the board**,
whatever put it there — `Puzzle::Do_ResovleLaneCrash` is a shared funnel, and abilities feed it too: Eraser's Undo
(one call per block returned), Pointman's pop, Pusher's push, Vegas's free power-up, grenades. An Eraser who erases 15
yellows and undoes them adds 15 "collected". The ratio goes past 100 %. Meanwhile blocks that went to Pointman's
stack or were erased never reach the board, so they are missing.

The game itself barely suffers — the ratio is only tested at ≥ 0.95, outside Mono, and the error favours the player —
but a display built on it is wrong. Use `Do_CollectCar` instead. If you must tell real collections apart at the board,
`Puzzle::LaneCrash_CarID` (#26) is set before every call: the traffic index for a real collection, `-1` for every
re-injection.

### Reading a colour as the player sees it

The colours on the road come from `XX_StartHere::FetchColorByID` (#882), and there are several palettes behind it.
In normal play the live one is branch #440, which imports **`XX_gui::Color1` … `Color5` in reverse order** (`ColorID` 0
purple → `Color5`, 4 red → `Color1`); green and yellow switch to `StartGroup::GlobalMonoColor` in Mono and Freeride.
Branch #1535 (`StartGroup` #1542 … #1526) is a **fallback**, live only when `Color5` is zero — writing there changes a
channel and nothing on screen. Mech and Portal have their own (`Highway::MechColor*`, `PlayerCar_Sword::PortalColor*`).

All are `Value Vector`s, read with `tw.vector_ch`. A write holds only on **leaves**: `XX_gui::Color1`'s components are
fed by the colour sliders' `Envelope`s, which overwrite a written value on the next recomputation — measured, every
second frame. A channel with something plugged into it is derived; only leaves keep what you write.

## `Environment/Puzzle` — the board

The board is the table `Puzzle_State` — `Blocks` (the colour in each cell, `-1` empty), `ColCount`, `MatchID`,
`MatchedColors`, `TileAge` and fifteen more columns; columns are lanes (`StartGroup::NumLanes`), rows `NumBufferRows`
(the graph calls the board the *buffer*). Six Lua scripts do what the graph does badly:

| Script | # | Does |
|---|---|---|
| `FindMatches` | 235 | finds matches by flood fill over 4 neighbours, wilds and stones handled |
| `SingleRowLowerBlocks` | 291 | lets blocks fall by one row |
| `DetonateBomb` | 538 | detonates by colour |
| `FindJollyRoger`, `FindPirateShapes` | 800, 1008 | leftovers — not in the game |

### The collection pass and the chain

There is no "chain timer". There is a **collection pass**, run when `MatchTimer` passes the window:

```
Do_ManageCollectionTimer (#455):
    MatchTimer += PausableTickCount        unless blocks are sliding (BlocksSlidThisFrame?) - then it holds
    if MatchTimer > SpecialPurpose::MatchCollectionTicks:
        Do_CollectMatches (#112):  MatchTimer := 0, FindMatches, then
            matches found -> score them, Do_IncrementChainCount (#1437): ChainCount := MIN(100, ChainCount + 1)
            none          -> Do_GraduallyResetChain (#1500): Casual 0, Pro FLOOR(ChainCount / 2), Elite 0
```

- **A tick is 1/25 s** (`StartGroup::PausableTickCount`, zero while paused). `MatchCollectionTicks` is 10 / 10 / 20 by
  league, overridden per character (Mono 15, Mono Pro 17, Eraser Elite 25, Mono Ninja 28, `Do_Berserker` 35, Freeride
  50) and by a song tag (#1378, clamped to 5–400). Read the channel; a number written into a script is right for one
  character.
- Time left before the chain is judged is `MatchCollectionTicks - MatchTimer`; clamp both ends.
- **Read that countdown every frame, never time it yourself.** It stands still while blocks slide — through a whole
  collapse — and a clock of your own keeps running and promises the player time they do not have. For the same reason
  do not smooth it: a smoothed countdown is a number that is not the time left.
- A second flag also holds the timer, `SpecialPurpose::HintmanEliteFreezingMatchCollection?` (#921). The only thing
  that sets it, `Do_IncreaseMatchCollectionTimer` (#718), sits behind a right-mouse `Trigger` (#866) that nothing in
  the group is wired to — in the Steam release it looks like a leftover. Reading `MatchTimer` covers it either way.
- An overfill (`Do_OverfillPunish`, #1148) resets the chain to zero.

**What a match is worth** is set by `Do_SetMoneyMultiplier` (#94):

```
ScoreBonus (#92) := 1
if MultipleColorsMatched? : ScoreBonus += 0.3                 the Sync Bonus  (+ Do_AddCombo)
if ChainCount > 0         : ScoreBonus += Envelope #643(ChainCount)        (+ Do_AddChainReaction)
```

`Envelope` #643 has six keys — `ChainCount` 0, 1, 4, 10, 20, 100 → 0, 0.5, 1.5, 2.0, 2.5, 3.0 — so the chain multiplies
by 1.0 … 4.0. It has no name; read it by index and check it against those keys. An `Envelope` is a numeric channel:
`tw.float_ch("Puzzle", 643):get()` is the curve applied to the current `ChainCount` — read the game's curve rather
than copying its keys.

The game's chain bar is the same curve: `XX_gui::Timer1` (#3938) is `Inertia((15.1041 / 3) × Envelope #12477(ChainCount))`,
and #12477 has #643's keys exactly. So the bar is **linear in the bonus, not in `ChainCount`** — that is the bar
players have learned to read at a glance, and a replacement that grows with `ChainCount` looks wrong to them. The
`Inertia` is why it slides rather than snaps.

### Points on their way: the score particles

A collected match does not add to the score. `Puzzle::Do_LaunchMoneyParticles` (#95) launches **one particle per
collected tile** to `MoneyFloaterCommander` (a seven-block match is seven launches with the same colour and value),
each with its own random speed; the score grows when each lands (`Do_EndLifeApplyScore` #198 → `Do_UpdatePoints`). The
game's "+N" texts are built afterwards from a per-frame difference of `PointsByColor`, which merges them per colour —
anything hooking the launch must merge them too. Read the `in*` channels of `MoneyFloaterCommander`, not its
`Launch_*` parameters (never wired). The score text is `ROUND(Inertia(StatCollector::Points))`.

So in the stock HUD the score grows first and its "+N" appears a frame later. A display that wants the number to
fly in and *then* the total to take it needs no timer of its own: the game already holds the points back for the
particle's flight. Announce at `Do_LaunchParticle` (#27), and let the total follow `Points`. A merged entry is done
when the **last** of its particles has landed: `Do_EndLifeApplyScore` runs inside the particle loop, so
`MoneyFloaterCommander::Index_Particles` is on the row that landed.

Not every point flies. Mono's per-block payment (`Do_HitColor` #1228), the penalties (`Do_SubtractPoints` #1205),
`BusyBee`, the power-ups and boss damage call `Do_UpdatePoints` directly. To tell them apart, hook `Do_UpdatePoints`
too: a landing particle's points come from `Do_EndLifeApplyScore` itself, which makes the `Do_UpdatePoints` call, so a
counter bumped in a `"before"` hook on it and consumed in the `Do_UpdatePoints` hook separates "landed" from
"direct" exactly. Take the amount as `Points` after minus `Points`
before — two reads, nothing to trust, and it is what the player's score actually moved (the `MAX(0, …)` clamp
included). The colour can be stale (above); draw an unknown one neutral rather than guess.

## `Actors/SpecialPurpose` — characters

Characters are branches of a `CallSelected` here, not the `PlayerCar_*` groups (those are models only):

```
Do_SpecialPurpose (#1)
  -> Do_SetAllConinuousPowers_OFF (#28)      every ability flag to 0 (typo the game's)
  -> Do_ForceCharacterChanges (#1432)        song tags such as [as-monoonly]
  -> Do_AllCharacters (#1079): CallSelected by StartGroup::PlayerVehicleType
```

Everything is switched off first and the character's branch switches its own things on, so a character's abilities
are exactly what its branch sets. The table of branches, with the names the game shows, is in
[Playing Audiosurf](playing-audiosurf.md#characters). Abilities, the stack, Eraser's memory, the Butler (Clean Match)
are described there with their channels.

## `Actors/Player` — the car

| Channel | # | |
|---|---|---|
| `CurrentPlayerRing` | 574 | the ring the car is on — the run's main cursor |
| `PlayerRingInterpolation` | 571 | position between rings |
| `PlayerLateralOffset` | 50 | sideways offset (on the shoulder or not) |
| `CarBodyMotion` | 5 | the car's matrix |
| `CurrentInputMethod` | 827 | mouse (< 1) or keyboard |
| `AutopilotOn?` | 977 | Freeride's autopilot, toggled by SPACE |

## `gui/XX_gui` — the HUD

Everything the player sees over the track during a run hangs off `Do_GUI` (#384); the table of nodes is in
[Playing Audiosurf](playing-audiosurf.md#the-screen-during-a-run). What to switch off, and what not to:

| To hide | Mute | Never mute | Why |
|---|---|---|---|
| medals | `Do_LadderMedalRequirements` #3464, `Do_MedalMarkers` | `Do_CalcMedalRequirements` | the latter writes the thresholds |
| the score, its top frame and the "+N" texts | `Do_ShowScore` #2351 | — | one cut; what it writes (`LastKnownPoints`, the `PointFloaters` table) only it reads |
| the chain bar | `Timer1` #3938 | `Do_ShowChainBar` #398 | under it `Do_ShrinkingBars` (#3936) also holds the song progress bar and a `Z - Buffer Clear` the HUD after it is drawn against |
| the song progress bar | `Timer2` #3970 | `Do_ShowChainBar` #398 | as above |
| the song title | `Do_ShowSongName` #2955 | — | writes only `SongnameX`, `SongnameEmissive`, `SongNamePositionSet?`, read by nothing outside |
| the two Double Vision scores | `Do_CooperativeModeSeparateScores` #3707 | — | two `CloneScoreText` under `SpecialPurpose::useClone?` |
| the flying "coins" | `MoneyFloater_GiveAlpha` (#4 in `MoneyFloaterCommander`) | `Do_RenderMoneyParticles` #3980 | it is the loop that **awards the points**: muting it stops scoring |
| the Best Score popup | `If` #5412 | `Do_ShowPersonalBest` #5348 | it also writes `PB` (#731) and holds Freeride's autopilot key (`Trigger` #6721); cutting at #5412 keeps `PB` fresh |
| the minimap | `Do_OverheadMap` #4147 | — | it writes `Highway::RingIndex`, but every reader sets that again first |

`Do_RenderMoneyParticles` is the trap of this table: it looks exactly like the render branches around it and is the
only place the score of a match is paid. Hide the coins inside the loop, never the loop.

The song title has a Tweaker tweak of its own (`hidden_song_title`, on `XX_WindowState::ShowSongname?`, which
`If` #6378 above #2955 tests). A script cannot see or set the host's tweaks; it does not need to — mute #2955: with
the tweak on, the game already skips the call; with it off, the mute catches it; the mute goes with the script.

**Dead branches that look alive** — easy evenings to lose: `Do_TripDisplay` (#248) draws what looks exactly like a
song progress bar and is reached by nothing; `DoGasGuage`, `Speedometer`, `SteeringWheel` are leftovers of a driving
prototype; `Do_LevelDisplay` (#4316) has no children. A channel that exists and looks plausible is not proof the game
uses it.

## How to check any of this

```
cgrpy query dec StatCollector Do_CalculateFinalStats     the end of the run
cgrpy query dec Puzzle Do_SetMoneyMultiplier             what a match is worth
cgrpy query dec TrafficCommander Do_CollectCar           the collection funnel
cgrpy query switch Achievements 1248                     colour IDs -> totals
cgrpy query writers StartGroup GoldRequirement           medal thresholds
cgrpy query users Do_UpdatePoints                        every caller of the scoring funnel
cgrpy query dec Puzzle Do_NinjaHitBonus --depth 4        Mono: what a collected block pays
cgrpy query switch SpecialPurpose 1155                   Mono: the step per league
cgrpy query switch SpecialPurpose 1117                   Mono: the cap per league
cgrpy query switch StatCollector 757                     Mono: what a matched block is worth
cgrpy query dec TrafficCommander Do_CollectQuestionBox   the power-up branch and its ghost
cgrpy query dec Puzzle Do_ManageCollectionTimer --depth 4   what holds the collection timer
cgrpy query writers SpecialPurpose MatchCollectionTicks  the collection window, per character
cgrpy query dec MoneyFloaterCommander Do_EndLifeApplyScore   a landing particle pays the score
cgrpy query dec XX_gui Do_ShrinkingBars --depth 1        the chain bar, the song bar, the Z-buffer clear
cgrpy query dec XX_gui Do_ShowPersonalBest --depth 2     PB, the popup, Freeride's autopilot key
cgrpy query dec XX_gui 6378 --depth 1                    the song title behind the tweak's flag
```

In a script, watch rather than guess: hook a handler with `tw.on_call` and `tw.notify` when it fires
([Reacting and intercepting](../scripting/hooks.md)).
