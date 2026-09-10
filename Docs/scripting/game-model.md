# How Audiosurf is built

[← back to the index](../scripting.md)

This is the chapter that makes the rest useful. The API is small; knowing *what to ask it for* is the
actual skill, and that means understanding how the game is put together.

## The game is a graph, not a program

Audiosurf is built in **Quest3D**, a visual engine where you do not write code — you wire up a graph.
The whole game, all of it, is roughly 131 000 nodes in that graph. There is no `main()`. There is a
tree of nodes, and every frame the engine walks it.

A node is called a **channel**. One channel might be:

- a number that something wrote into it (`Points`)
- a formula over other channels (`A+B`, `MAX(0, x)`)
- a piece of geometry that draws itself
- a texture
- a *handler*: a channel whose job is to call its children in order, which is how the game expresses
  "do these things, in this sequence"

Every channel has a **name**, a **type**, and a list of **children** — the other channels wired into
its inputs. That is the whole model.

### Two ways a channel is used

This distinction matters more than any other, because it decides which part of the API you reach for.

**Evaluated for a value.** You ask the channel what it holds and it answers — computing it from its
children first if it needs to. `StatCollector::Points` evaluates to a number. `Feat String` evaluates
to text.

> In scripts: `tw.float_ch`, `tw.string_ch`, `tw.vector_ch`. See [Reading and writing the game](channels.md).

**Called as an action.** The game runs the channel for its effects. Nothing meaningful comes back;
the point is what it did. `TrafficCommander::Do_CollectCar` is called when your car touches a block,
and what it *does* is remove that block from the road and put it somewhere.

> In scripts: `tw.on_call` and `tw.mute`. See [Reacting and intercepting](hooks.md).

Some channels are both. That is fine and normal.

### Naming conventions the game actually follows

The game's authors were consistent enough to be useful:

| Pattern | Means | Example |
|---|---|---|
| `Do_Something` | an action handler — something happens when it is called | `Do_ResetStats` |
| `Something?` | a flag, 0 or 1 | `GamePaused?`, `Ninja?` |
| `Index_Something` | a **cursor** into a table (see below) | `Index_TrafficPattern` |
| `Table: Column` | one column of a table | `Stats: TrafficColorCounts` |
| `Fetch_Something` | a value pulled from somewhere else | `Fetch_NumberBlocksInPlay` |

Nothing enforces this. But when you are hunting for something, guessing `Do_` + what you want is
often right.

## Groups

The graph is split across ~161 files called **groups** (`.cgr` files in the game's `engine/` folder).
A group is a namespace and a unit of loading.

You address a channel as **group + name**:

```lua
tw.float_ch("StatCollector", "Points")
```

Groups reference each other, and the game writes that as `StatCollector::Points` in its own tooling.
Same thing.

**Groups load and unload as you move around the game.** `XX_PauseScreen` is not loaded while you are
in the main menu. This is the single most common reason a script "does not work" — and it is why
channel handles resolve lazily and keep retrying rather than failing once at startup.

To see what is loaded right now, enable the **Group Inspector** script that ships with the Tweaker,
or call `tw.groups()` yourself.

### Group names are a little loose

A group has a file path (`Scores/StatCollector.cgr`) and a pool name (`StatCollector`). The Tweaker
will match either, plus the bare file name without path or extension. In practice: **use the short
name**, `"StatCollector"`, and it works.

One name is not what you would guess: the root group `XX_StartHere.cgr` is called **`StartGroup`**.
Both work; the game's own cross-references say `StartGroup`.

## Tables and cursors

The game stores lists — the generated track, per-colour counts, playlists — in **Array Tables**. This
one is genuinely strange the first time and worth reading slowly.

A table column is an `Array Value` channel. But it takes no index argument, because channels do not
take arguments. Instead:

- the column has a separate **cursor channel** wired into it (conventionally `Index_Something`);
- you write the row number into the cursor;
- then you evaluate the column, and it gives you that row.

The game does exactly this in its own logic — a loop writes 0, 1, 2… into the cursor and reads the
column each time.

Scripts do not do that dance by hand. `tw.array` does it for you, and importantly **puts the cursor
back** afterwards, because the cursor belongs to the game and leaving it moved corrupts whatever the
game does next:

```lua
local counts = tw.array("StatCollector", "Stats: TrafficColorCounts", "Index_TrafficColorCounts")

local reds = counts:get(4)
```

For columns of vectors, `tw.array_vec` does the same and gives you three numbers.

## The traps

These are not hypothetical. Every one of them cost real debugging time.

### Channel names are not unique

Within a single group, two channels can have the same name. `TrafficCommander` has **two** `Value`
channels called `TrafficType`, and a lookup by name finds the first one — which is not the one the
collision handler writes.

When a name is ambiguous, address the channel by its **index** instead:

```lua
local type_ch = tw.float_ch("TrafficCommander", 900)   -- the right TrafficType
```

The catch: an index is a position in a specific build of the game. It is more precise and more
fragile. **Prefer the name wherever the name is unambiguous**, and reach for an index only when you
have established it is not.

### Some channels look readable and are not

A channel that is a **group parameter** — an input the group receives from whoever called it — reads
as whatever it happened to hold last, not as the value being passed. `Do_ReportCarCollected` takes a
colour that way, and a script reading that channel gets the same stale number every time. It looks
like it works. It does not.

If a value seems frozen or implausible, suspect this, and look for where the value actually comes
from rather than where it is consumed.

### A value's meaning can depend on when you read it

Some of the game's state is only assembled at the end of a run. `Stats: TrafficColorCounts` is
written **once, in the final scoring pass** — during a run it is stale, and it never contains white
blocks at all. A script that reads it mid-run gets a plausible wrong answer.

When accuracy matters, find where the game gets the number from, not where it stores it.

## How to find the thing you want

You will not find most of this by guessing. The workflow that works:

**1. Start from what the game shows you.** If you can see a number on screen, it exists in a channel,
and it usually has an obvious name.

**2. Read the reverse-engineering notes.** `Docs/Internal/reversing-journal-gameplay.md` maps out the
gameplay-critical groups in detail — the whole of `StatCollector`, the scoring rules, the character
and mode table, the puzzle grid. It is in Russian and it is working notes rather than polished
documentation, but it is the map, and it names channels with their indices.

**3. Use the Group Inspector.** When a group will not resolve, the answer is almost always "it is not
loaded right now" or "it is called something else", and the list tells you which.

**4. Watch, do not guess.** Hook a `Do_` handler with `tw.on_call` and have it call `tw.notify` when
it fires. You will learn very quickly whether it means what you thought.

```lua
tw.on_call("TrafficCommander", "Do_CollectCar", function()
    tw.notify("collected!")
end)
```

## A starting map

Verified working, and enough to build most things. Times when each is meaningful are noted, because
that is the part that bites.

### `StatCollector` — the run

| Channel | Kind | Notes |
|---|---|---|
| `Timer` | number | Seconds into the song. Resets with the run. |
| `Points` | number | Running score, **without** end-of-run bonuses. |
| `LargestMatch` | number | Biggest match so far — drives the Match7/11/21 bonuses. |
| `TotalCarCount` | number | Blocks on the generated track. Grows *during* track generation. |
| `NumTilesInGrid` | number | Blocks currently on your board. `0` means a Clean Finish is live. |
| `PointsWithGridBonus` | number | Final score including bonuses. Only meaningful after the run. |
| `HighestMedalEarned` | number | 0 none, 1 bronze, 2 silver, 3 gold. After the run. |
| `Feat String` | **text** | Bonuses earned, comma-separated. After the run. |
| `Do_ResetStats` | action | The **track** is generated. Once per song — *not* on a restart. |
| `Do_ResetSimpleStats` | action | The **run** starts. Every entry into gameplay, restart included. The "run began" event. |
| `Do_ReseteWhiteWildBlocks` | action | Whites, wilds and power-ups are placed over the generated track. Re-randomised on every run. (The typo is the game's.) |
| `Do_CalculateFinalStats` | action | Fires when a run ends and scoring happens. |

The first three are three different moments and picking the wrong one is a classic mistake — see
[Reacting and intercepting § Events worth knowing about](hooks.md#events-worth-knowing-about).

### `StartGroup` — the game as a whole

| Channel | Kind | Notes |
|---|---|---|
| `StartupState` | number | Which mode the game is in. |
| `State_Gameplay`, `State_MainMenu`, `State_SongSelector` | number | The constants to compare it against. Comparing beats hardcoding. |
| `BronzeRequirement`, `SilverRequirement`, `GoldRequirement` | number | Score thresholds for this track and difficulty. |
| `LeagueID` | number | Difficulty tier, 0-based. |

"Is the player actually playing?" is `StartupState == State_Gameplay`. That is the game's own test.

### `XX_PauseScreen` — pause

`GamePaused?` (number, 0/1), and the actions `Do_Pause`, `Do_Unpause`, `Do_TogglePauseState`. Only
loaded during gameplay — do not ask for it from the menu, or you will collect warnings about a group
that is legitimately absent.

### `SpecialPurpose` — the character you picked

`Ninja?`, `Freeride?`, `DumptyScoopDown?` (Pointman's buffer is down), `ShatterStorming?` (Eraser is
shattering). These are how you tell *which* ability just consumed a block.

`MatchCollectionTicks` is here too — how long the board waits between match-collection passes. It is
per character *and* per league (10/10/20 by league, overridden to 15 for Easy Ninja, 50 for Freeride,
and a dozen others besides), and a song tag can set it outright, so read the channel rather than
assuming a number.

### `Achievements` — per-colour hit counters, and the rating

`PurplesHit`, `BluesHit`, `GreensHit`, `YellowsHit`, `RedsHit`, `WhitesHit`, `TotalBlocksHit`. Live
during a run. These are the counters the game's own 95%-of-a-colour bonuses are tested against.

`SkillRating` is also here, written once at the end of a run by `Do_CalcSkillRating`:

```
MAX(1, ROUND(StatCollector::PointsWithGridBonus / StartGroup::GoldRequirement * 100
                                                * (StartGroup::LeagueID + 1)))
```

It is sent to the server and **never displayed**, which makes it a tempting thing for a HUD to
predict — and an unusually unforgiving one, because the player has nothing to check it against. Two
of the three terms are not what they look like: `GoldRequirement` is not a property of the song but
`TotalCarCount × {10, 30, 35}[LeagueID]`, recomputed per run, and `LeagueID` is a property of the
**character** (0, 1 or 2), not of the difficulty — that is `ChosenDifficulty`, and it selects a
different set of medal thresholds entirely.

`PointsWithGridBonus` is `Points` plus one `Points × scaler` term per feat earned — all over the raw
`Points`, never compounded. The Clean Finish term is included whenever `NumTilesInGrid == 0` **at the
moment scoring happens**, so a live prediction should count it whenever the board is currently
empty, not treat it as a hypothetical.

### `TrafficCommander` — the road

`Do_CollectCar` fires whenever your car takes a block off the road — through the grid, through
Pointman's buffer, or through an Eraser shatter. It is the single honest "I collected something"
event. The colour is in channel index `900` (the name `TrafficType` is ambiguous; see the traps).

### `Puzzle` — the board

`LaneCrashColor`, `Fetch_NumberBlocksInPlay`, and the grid machinery. See the internal notes.

The chain — the multiplier the game shows as a growing bar — lives here too, and it is worth knowing
that none of it is what its names suggest:

| Channel | Kind | Notes |
|---|---|---|
| `ChainCount` | number | Consecutive collections, capped at 100. **Not** the multiplier. |
| *(index 643)* | number | The `Envelope` that turns `ChainCount` into the bonus. Six keys, 0/1/4/10/20/100 → 0/0.5/1.5/2/2.5/3, so the multiplier runs 1.0 to 4.0. It has no name; read it by index and cross-check it against those keys. |
| `MatchTimer` | number | Ticks since the last match-collection pass. Frozen while blocks are sliding. |

There is no "chain hold" timer. What exists is the collection pass, which runs every
`SpecialPurpose::MatchCollectionTicks` ticks — and the chain is whatever survives it: a pass that
finds no match drops the chain, to zero or to half depending on the league. So the time left before
the chain is judged is `MatchCollectionTicks - MatchTimer`, and both ends of that need clamping: the
counter can sit a frame past the window, and can be pushed below zero while collection is suppressed.

**Ticks are 25ths of a second.** Every second-valued counter in the game divides by 25 —
`StatCollector::Timer` is `Timer + StartGroup::PausableTickCount/25`.

## Next

[Reading and writing the game](channels.md) — turning all of this into actual calls.
