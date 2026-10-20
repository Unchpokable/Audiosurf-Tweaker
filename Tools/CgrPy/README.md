# CgrPy

Tools for reading Audiosurf from the outside: its Quest3D channel graphs (`.cgr` files) and the DLLs that
implement them. Almost everything Audiosurf Tweaker knows about the game was found with these, and they are
how you find what nobody has written up yet — for a script, for a mod, or just to know.

They **read** the game. They change nothing in it, with one exception that refuses to touch a live install
(`meshes replace`, below).

## Where it is

- In **Audiosurf Tweaker**: `DevTools\CgrPy\` next to `TweakerUI.exe`.
- In the Tweaker's **source repository**: [`Tools/CgrPy/`](https://github.com/Unchpokable/Audiosurf-Tweaker/tree/HEAD/Tools/CgrPy).

## Setup: uv

You need **[uv](https://docs.astral.sh/uv/)** on your PATH, and that is all:

```
winget install --id=astral-sh.uv -e
```

Open a new terminal afterwards so it sees uv. The first run of any tool sets up Python and the two packages
CgrPy uses (`capstone`, `pillow`) by itself, which needs the internet once. Everything that creates —
the virtualenv, uv's package cache, the Python it may download, and CgrPy's own cache of the parsed game —
stays **inside this folder** (`.venv\`, `.cache\`). Nothing goes into your user profile, and deleting those
two folders undoes it all.

## Running a tool

```
cgrpy                              list the tools, one line each
cgrpy <tool>                       a tool's usage (except build_proj and chtypes, which just run)
cgrpy query find "Undo"            ...and so on
```

How you type `cgrpy` depends on where you are:

| Where | Type |
|---|---|
| A terminal in an editor opened from the Tweaker's **Script Dev** tab | `cgrpy` — this folder is on that terminal's PATH |
| cmd or PowerShell, in this folder | `.\cgrpy` (cmd) or `.\cgrpy.cmd` (PowerShell) |
| Git Bash, in this folder | `./cgrpy` |
| Anywhere else | the full path to `cgrpy.cmd` / `cgrpy` |

The `.\` is not optional in cmd when `NoDefaultCurrentDirectoryInExePath` is set (AI coding agents set it):
cmd then does not look in the current folder at all.

Arguments arrive exactly as typed: a regex with `|` in it, or a type name with a space, needs only the quotes
you would use anyway.

## Finding the game

No tool has a path to the game written into it. They look, in this order:

1. **`AUDIOSURF_DIR`** — the Audiosurf folder, or its `engine` folder. The Tweaker sets it for an editor it
   opens.
2. **The Tweaker's own setting**, when CgrPy runs from `DevTools\CgrPy`: the game folder you chose in the
   Tweaker.
3. **`<repo>\Audiosurf_Steam`**, when CgrPy runs from the source repository.
4. **Steam**: the Steam folder from the registry, and every library in its `libraryfolders.vdf`.

If none works, the error says where it looked. Set `AUDIOSURF_DIR` and run again.

## Two kinds of tool

**Binary tools** — `chtypes`, `vtdump`, `findvt`, `symbols`, `disasm` — read a DLL or EXE directly. Nothing to
prepare, instant, and they answer questions about *code*: which slot a method is in, what a class derives
from, what a function actually does.

**Graph tools** — `query`, `render`, and the scripts in `examples\` — answer questions about *data*: which
channel feeds which, who calls this, who writes that, where a value really comes from. They read a parsed
snapshot of the whole game, so build it once:

```
cgrpy build_proj
```

```
157 groups, 131255 channels, 0 errors, 4.9s
unparsed bytes: as expected (15 groups with known losses, 3141952 bytes)
```

It takes seconds and writes `.cache\proj.pkl` (~200 MB). **Read the second line.** The parser walks a stream of
self-describing chunks, and if it ever lost its place it would not stop — it would go on producing plausible
nonsense. A few asset-heavy groups always lose some trailing bytes, and `build_proj` knows exactly which and
how many. Anything beyond that it names, group by group, as a warning: treat it as a parser bug, or a game file
that is not the one the baseline was measured on, and do not trust answers about that group.

If the game's files change afterwards (an update, a patch of your own), every graph tool warns that the cache
is stale until you run `build_proj` again.

`meshes` reads the `.cgr` files directly and needs no cache.

## The tools

Every tool prints its usage, with where it was used and why it helped, when run without arguments.

### `query` — the questions every investigation starts with

```
cgrpy query find     <regex> [--group G] [--type T] [--stubs]
cgrpy query dec      <group> <channel> [--depth N]     a handler as pseudocode
cgrpy query value    <group> <channel>                 what a channel evaluates to
cgrpy query writers  <group> <channel>                 everything that writes it, in any group
cgrpy query switch   <group> <channel>                 a ChannelSwitch: selector and every branch
cgrpy query parents  <group> <channel>                 what it is wired into
cgrpy query users    <name>                            every group that imports a channel by that name
```

A **group** is a path, a file name or a short name: `Scores/StatCollector.cgr`, `StatCollector`, and
`StartGroup` for the root group `XX_StartHere.cgr`. A **channel** is an index (`798`, `#798`) or a name — and
a name more than one channel carries is refused, with their indices:

```
> cgrpy query dec TrafficCommander TrafficType
query: 2 channels in Actors/TrafficCommander.cgr are called 'TrafficType': #631 [Value], #900 [Value] - give the index
```

Worked examples, all from checking [Playing Audiosurf](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/internals/playing-audiosurf.md) against the
game:

```
> cgrpy query switch StatCollector 128          what a block of each colour is worth
Scores/StatCollector.cgr #128 [ChannelSwitch] 'ChannelSwitch'
  selector: #121 = Index_ColorPointValues
  [0] #129 'Value' = 1
  [1] #130 'Value' = 2
  [2] #131 'Value' = 3
  [3] #132 'Value' = 5
  [4] #133 'Value' = 8
  ...

> cgrpy query writers StartGroup ChosenDifficulty
  Intros/MainMenu.cgr #250: ChosenDifficulty := 0
  ...
  gui/XX_PauseScreen.cgr #477: ChosenDifficulty := (FLOOR(Slider Value(Slider Pos)*(VehicleSelector::NumDifficulties-1)))
  ...
8 writer(s) of XX_StartHere.cgr #431 [Value] 'ChosenDifficulty' = 1

> cgrpy query dec EraserEffect Do_FireUndo      how Eraser's Undo puts blocks back
```

### `render` — the draw path of a scene node

```
cgrpy render find     <group> <name>     channel indices matching a name - every match, never just the first
cgrpy render chain    <group> <index>    down: 3D Object -> Surface -> geometry / material / texture
cgrpy render callers  <group> <index>    up: what invokes it each frame, following imports between groups
cgrpy render texusers <group> <index>    which Surfaces use this texture
```

`find RenderCommon GridColor` returns **three** channels of that name; `callers RenderCommon 202` lands on
Material `ro_01` under Surface `ro_01 1` — road furniture, not the puzzle tiles it was assumed to colour.

### Binary tools

| Tool | What it answers | Example |
|---|---|---|
| `chtypes` | Which DLL implements a channel type. A type name means nothing to a disassembler; this is the first step of every binary question about a channel. | `cgrpy chtypes "Array V"` → `Array Vector  DD626E09-….dll` |
| `vtdump` | A class's vtable, slot by slot, named from the exports | `cgrpy vtdump --type "Array Vector"` — `SetVector` in slot 18, `SetFloat` in 19 |
| `findvt` | Vtables nothing exports, found by their shape | `cgrpy findvt --type "Array Unique" --min 23 --ret 20:4 --ret 22:0xc` — exactly one candidate, `ArrayConnectItem` |
| `symbols` | A module's exports and imports. Imports are the cheap way to find a class's parent: a channel DLL imports its base's constructor. | `cgrpy symbols --type "Array Vector" --imports` → `??0Aco_VectorChannel@@QAE@XZ` |
| `disasm` | What a function actually does, calls named | `cgrpy disasm --type "Array Vector" 0x1240` — `SetVector`, three dwords from `[esp+4]`, `ret 0xc` |

### `meshes` — geometry

```
cgrpy meshes inventory  [engine_dir] [out.csv]     every mesh blob -> a CSV (default out\meshes.csv where you are)
cgrpy meshes export     <file.cgr> <ordinal> <out.obj>
cgrpy meshes exportall  <file.cgr> <out_dir>
cgrpy meshes replace    <file.cgr> <ordinal> <in.obj> <out.cgr> [--not-live]
cgrpy meshes verify     [engine_dir]               930 blobs: 893 re-encode byte-exact, 930 OBJ round-trip
```

`ordinal` is a blob's position in the file, as `inventory` and `exportall` print it — not a channel index; one
channel can hold several blobs.

**`replace` writes a `.cgr`, and a broken `.cgr` is a broken game.** It refuses to write into anything that
looks like the Steam install — "steam", "steamapps" or the app id 12900 in the path, or a Steam `.vdf`/`.acf`
file in a folder above. Work on a copy, and copy the result into the game yourself, knowing that Steam's
*Verify integrity of game files* puts the original back. A copy that only looks live (a folder called
`Audiosurf_Steam`, say) takes `--not-live`.

## Gotchas

**Names lie.** A great many of the game's channels are named after something they no longer do, or after
nothing; whole families (`FindJollyRoger`, "candy cane / sword / hook / skull") are leftovers. A name is where
to start looking. What a channel *is* is who writes it, who reads it, and what a probe script shows in the
running game.

**The game's words are not the player's.** The board is the *buffer* (`NumBufferRows`), Eraser's erase is
*shatter*, Clean Match is the *Butler*. [Playing Audiosurf](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/internals/playing-audiosurf.md) maps
one onto the other.

**Names are not unique.** Within one group several channels can share a name. Every tool that takes a name
either prints every match or refuses to guess. When you write an index down, write down why that one.

**A run of code pointers is not one vtable.** MSVC lays vtables back to back with nothing between them, so
`findvt` and `vtdump` can run past the end of the class you asked for. The real end shows in the names (the
class prefix changes) or as the type's own name appearing as a string.

**`proj.pkl` is a cache of someone else's game.** Never commit it or pass it around; rebuild it.

**`Cache\` under `engine\` is left out.** It holds Audiosurf Radio songs the game downloaded — a different set
for every player — not the game's logic.

## `examples\`

The normal way to use the graph is not "run a tool" but "write fifteen lines against `cgr.project` and
`cgr.decomp`, get an answer, throw it away". These three show what that looks like, and run as they are:

```
cgrpy examples\writers.py      who writes each run statistic (writes out\stat_writers.txt where you are)
cgrpy examples\qchic.py        reading a chunk the parser does not keep (CHIC, the memoisation flag)
cgrpy examples\genall.py       CSVs and pseudocode for a set of groups at once (into out\)
```

They hardcode the groups they were written for: copy one and change that. In a script of your own,
`from cgr.cache import load_project` gives you the parsed game, and `P.find_group("SpecialPurpose")` a group.

## The package

| Module | Role |
|---|---|
| `cgr/core.py` | container: zlib + XOR 0x04 peel, chunk walk, `channels.lst` |
| `cgr/graph.py` | graph parser with lookahead framing |
| `cgr/project.py` | all groups, cross-group import resolution, `find_group`, `group_params` |
| `cgr/cache.py` | the parsed-project cache: loading it, and noticing when it is stale |
| `cgr/decomp.py` | graph → pseudocode |
| `cgr/mesh.py` | `3D ObjectData` geometry: chunks ↔ arrays, OBJ ↔ mesh |
| `cgr/container.py` | lossless unwrap / rewrap of the zlib + XOR envelope |
| `cgr/patch.py` | locate mesh blobs by ordinal, splice replacements |
| `cgr/pe.py` | minimal PE reader: RVA ↔ offset, exports, imports, vtables |
| `cgr/chtypes.py` | `channels.lst` → type name → DLL |
| `cgr/paths.py` | where the game is, and `is_live_game`; the one place that knows |
| `launcher.py` | what `cgrpy` runs: finds the tool and runs it |

The import name is `cgr`. `pe.py` is hand-rolled rather than built on `pefile` on purpose: these files are read
next to a disassembler and notes that quote raw RVAs, and every layer that renames or normalises something is a
layer where an address in the notes stops matching the one on screen.

## Where the findings are written down

This folder is the tooling; what was found with it is written up here:

- **[Playing Audiosurf](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/internals/playing-audiosurf.md)** — the game from the player's seat, with
  every term tied to its groups and channels. Start here.
- **[How Audiosurf is built](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/scripting/game-model.md)** — channels, groups, tables, and a starting map,
  for script authors.
- **The reverse-engineering journals** — the full working notes, in Russian, rougher than the above. Each one
  says which commands produced its numbers:

| Journal | About |
|---|---|
| [gameplay](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-gameplay.md) | the `.cgr` format (§1), node semantics (§2), statistics, scoring, characters, the board, audio, the HUD |
| [engine](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-engine.md) | the native side: channel ABI, vtables, object layout, how a frame runs |
| [boot](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-boot.md) | start-up, loading order, groups unloading and coming back, channel families |
| [geometry](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-geometry.md) | meshes, chunks, the OBJ round-trip, the 930-model inventory |
| [render](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-render.md) | the track's visual environment, the order of a frame |
| [lua](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-lua.md) | the game's own Lua engine |

If a tool here and a journal there disagree, the journal is the claim and the tool is the evidence — run it
again before believing either.

What is not here, and never will be, is the game itself: these tools read someone else's files, they do not
ship them.
