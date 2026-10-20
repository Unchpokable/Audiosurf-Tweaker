# Start-up and the life of groups

[← back to the index](../internals.md)

When does the channel graph start meaning anything, and does it stay put once it does? Neither answer is
obvious, and both cost real bugs before they were worked out. This page is the timeline: one frame, then the
start of the game, then the rest of the session.

## One frame

The game's window (`engine\QuestViewer.exe`) runs an ordinary single-threaded message loop. When there is no
message to handle, it calls **`EngineControl::EngineLoop()`** in `engine\HighPoly.dll` — and that one call is the
whole frame of the graph:

1. the engine's frame counter goes up by one (`IncreaseTreeCount`);
2. the **start group** is looked up, and its start channel is called (`CallStartChannel`);
3. everything else the game does that frame happens because that channel called something, which called
   something else.

Two things follow:

- **Only one group is entered per frame — the start group.** The other ~160 run only when something reaches them
  through an import. A group nobody calls this frame does nothing this frame.
- **The graph, the window procedure and Direct3D's `EndScene` all run on one thread.** Nothing in the game's own
  logic runs concurrently with anything else in it.

The frame counter is what memoisation is measured against ([graph format](graph-format.md#reading-values-memoisation)):
"computed this frame" means "computed since the last `IncreaseTreeCount`". It wraps at 30 000 — compare frame
numbers for equality only.

## The start of the game

The first group to run is **not** the game. `engine\Q3DStart.q3d` names the start group, and it is
`start - project loader.cgr` — Quest3D's stock project loader with its helpers (`load group sequence.cgr`,
`progress calculator.cgr`, …). It shows the light-sensitivity warning and the loading bar while it loads the
groups listed in `XX_StartHere_config.xml` — 81 of them; the others are loaded on demand later.

When loading has finished, a channel of type `SetNewStartChannel` hands the engine over to
**`StartGroup::Do_StartFromExternal`** in `XX_StartHere.cgr`. The condition is the game's own definition of
"everything is loaded":

```
load group sequence::Progress (0-1) val >= 1   (the share of start groups loaded)
&& ReadyToAdvancePastNews?
```

(or the quick-start path, which also waits for loading). From that frame on, the start group is
`XX_StartHere.cgr`, pool `StartGroup`.

**That hand-over is the only exact sign that the game has loaded**, and it can be seen from outside: ask the
engine for its start group and compare its file name with `XX_StartHere.cgr`. It is what Audiosurf Tweaker's
script layer waits for before it runs a single line of a script — `tw.state()` reports it.

Even after the hand-over the game is not quite running yet: `Do_StartFromExternal` first reads `config.ini`
asynchronously (a few frames), and only then calls the game's real start channel, which among other things resets
`StartupState` and seven other flags. The real value of `StartupState` appears **after** the group does.

## Why a channel read too early lies

A `.cgr` stores every leaf channel with the value it had in the author's editor when the project was last saved.
That is not "zero" or "unknown" — it is a snapshot of the author's desk on the day the project was saved. Until the engine has computed a
channel, reading it returns that snapshot: a plausible number, and wrong.

| Channel | Stored in the file | Becomes real |
|---|---|---|
| `StartGroup::StartupState` (#855) | `2` — "main menu" | reset inside the game's start channel, after the hand-over |
| `StartGroup::ini_Initialized?` (#3764) | `1` — "config parsed" | set to 0 on the first frame of `Start`, then back to 1 once `config.ini` is read |

`ini_Initialized?` is the instructive one: read too early, it gives the right answer for the wrong reason, and
keeps doing so until the day it does not.

> **A channel's value means something only once the engine has computed it. Before that, you are reading the
> editor, not the game.**

For a channel that is memoised (`CHIC = 0`, about 90 % of them) there is a precise test: the engine writes the
frame number into the channel each time it computes it, so "the channel's last computed frame equals the group's
current frame" means "the engine computed this channel this frame". For `CHIC = 1` channels the field is never
written and the honest answer is "unknown". The field offsets are on the [native ABI](channel-abi.md#the-channel-object) page.

For scripts the rule is already enforced: their callbacks do not run before the game has loaded, and `tw.state()`
says what the layer is waiting for.

### "Is the player playing?"

| Question | Answer |
|---|---|
| Is the game loaded? | the start group is `XX_StartHere.cgr` (see above) |
| Is a run in progress? | `StartGroup::StartupState == StartGroup::State_Gameplay` — the game's own test, in `Stage/XX_WindowState::InGameplay?`. Valid only once the game has loaded. |
| Is it paused? | `XX_PauseScreen::GamePaused?` (the group exists only during gameplay) |

The state constants live beside `StartupState`: `State_MainMenu` 2, `State_CharacterSelect` **also 2**,
`State_SongSelector` 3, `State_Gameplay` 5, `State_Resetter` 7, `State_TeamManager` 8. Compare against the
constant channels rather than writing the numbers in. `StartupState`'s name is not unique in its group (#855,
#2373, #4165); imports resolve to the same first match as a lookup by name, so a lookup by name gets the channel
the game itself uses.

**Read states, hook transitions.** A flag assembled from hooks on start, pause and unpause is only right if it saw
every edge; something that attaches in the middle of a run starts wrong. Reading the state is right from the
first frame. Hooks are for moments (`Do_ResetStats`: the track was generated).

## Groups come and go all session

Loading does not end at start-up. Some groups are destroyed and loaded again while you play:

- **The `Renderer` pool is unloaded and reloaded on every run** (`XX_StartHere::Do_ResetLevelContent`: Remove
  Group `"Renderer"`, then one of `Render\Render1.cgr` … `Render9.cgr` is loaded into it).
- `FreeVersionAd` is another such pool.
- Groups like `XX_PauseScreen` exist only during gameplay.

So **a pointer to a channel of a dynamic group lives no longer than one run**. Anything that keeps one — a
script's channel handle, a hook on a channel — has to notice the group going away. In scripts the Tweaker does it:
a handle into an unloaded group reads `nil` and comes back by itself, a hook detaches and reattaches, and
`tw.on_group(name, fn)` reports it ([Reacting and intercepting](../scripting/hooks.md)).

How the game manages groups, for when you read its graph:

| Channel type | What it does |
|---|---|
| `Group Loader` | reads a file into a buffer, asynchronously |
| `Group Loader Status` | `0` while reading, `1` when done |
| `Load ChannelGroup` | loads a `.cgr` from a buffer into a named pool |
| `Remove Group` | unloads a pool — never its own group |
| `IsChannelLoaded` | whether a group is there |
| `GroupSelfSave` | saves a group to disk (options, saved constants, scores) |

The game's own Lua can load and remove groups too (`q.LoadChannelGroup`, `q.RemoveChannelGroup`), and can even call
a group's start channel — so "the start channel is called once per frame" is true of the engine, not a guarantee.

## How to check any of this

```
cgrpy query dec "start - project loader" 335         the hand-over condition (an If)
cgrpy query dec StartGroup Do_ResetLevelContent      the Renderer pool going away
cgrpy query find "^State_" --group XX_StartHere      the state constants
cgrpy disasm HighPoly.dll 0x10b10                    EngineControl::EngineLoop itself
```

In a script: `tw.state()`, `tw.groups()` and `tw.on_group()` show the same lifecycle from inside the game.
