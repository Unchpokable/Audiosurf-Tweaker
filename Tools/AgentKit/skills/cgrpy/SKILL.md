---
name: cgrpy
description: Finding things inside Audiosurf's channel graph with the CgrPy tools - which group and channel hold a value, what a Do_ handler does, who writes a value, what a switch selects, what a node is wired into; plus throwaway Python queries against the parsed game. Use when the docs do not name the channel, handler or value a script needs, and before any write or mute.
---

# CgrPy: asking the game's graph

Audiosurf's logic is a graph of **channels** grouped into **groups** (`.cgr` files), built in the Quest3D editor and
never meant to be read. CgrPy reads it. Most questions a script raises — "where is the number on screen", "what
does this handler do", "who else writes this" — are one `cgrpy query` away.

Background, in the order you will need it: `.tweaker/docs/scripting/game-model.md` (the model and how to look),
`.tweaker/docs/internals/playing-audiosurf.md` (player words → groups and channels),
`.tweaker/docs/internals/gameplay-map.md` (the gameplay groups, with indices and traps),
`.tweaker/docs/internals/graph-format.md` (what the node types do). The tool's own manual is `README.md` in the
CgrPy folder.

## Running it

- On `PATH` when the Tweaker opened this editor: `cgrpy`.
- Otherwise from `.tweaker/local.json`: `<devtools>\CgrPy\cgrpy.cmd` (cmd, PowerShell) or `<devtools>/CgrPy/cgrpy`
  (Git Bash). In cmd, a bare name in the current folder needs `.\cgrpy`.
- `cgrpy` alone lists the tools; `cgrpy <tool>` with no arguments prints its usage.
- It needs **uv**. If the launcher says uv is missing, tell the user to run `winget install --id=astral-sh.uv -e`
  and open a new terminal — do not install it yourself. The first run downloads Python and two packages; that
  needs the internet once.
- It finds the game by itself (`AUDIOSURF_DIR`, the Tweaker's setting, Steam). If it cannot, set `AUDIOSURF_DIR`
  to the folder from skill `audiosurf-game-files`.

## Once per game: build the snapshot

```
cgrpy build_proj
```

```
157 groups, 131255 channels, 0 errors, 4.9s
unparsed bytes: as expected (15 groups with known losses, 3141952 bytes)
```

**Read the second line.** "as expected" is good. Anything else names groups the parser lost its place in —
do not trust answers about those groups, and tell the user. If the graph tools warn that the cache is stale, run
`build_proj` again. Never commit or share the cache.

## `query` — start every investigation here

```
cgrpy query find     <regex> [--group G] [--type T] [--stubs]
cgrpy query dec      <group> <channel> [--depth N]     a handler as pseudocode
cgrpy query value    <group> <channel>                 what a channel evaluates to
cgrpy query writers  <group> <channel>                 everything that writes it, in any group
cgrpy query switch   <group> <channel>                 a ChannelSwitch: selector and every branch
cgrpy query parents  <group> <channel>                 what it is wired into
cgrpy query users    <name>                            every group that imports a channel by that name
```

A group is a path, file name or short name (`Scores/StatCollector.cgr`, `StatCollector`; `StartGroup` is the root
`XX_StartHere.cgr`) — the same short names `tw.*` takes. A channel is an index (`798`, `#798`) or a name; an
ambiguous name is refused with every index — then you choose, and in the script you use the index with a comment.

Typical moves:

| Question | Command |
|---|---|
| where is X | `cgrpy query find "Undo"`, narrowed with `--group` and `--type "Value"` |
| what does this handler do | `cgrpy query dec EraserEffect Do_FireUndo` |
| where does this value really come from | `cgrpy query writers StartGroup ChosenDifficulty` |
| what each branch of a switch is | `cgrpy query switch StatCollector 128` |
| is it safe to mute / what reads it | `cgrpy query parents XX_gui 2351` and `cgrpy query dec` on each parent |
| which groups use a shared channel | `cgrpy query users Points` |

## `render` — the draw path

```
cgrpy render find     <group> <name>       every match of a name, never just the first
cgrpy render chain    <group> <index>      down: 3D object -> surface -> geometry, material, texture
cgrpy render callers  <group> <index>      up: what draws it each frame, across groups
cgrpy render texusers <group> <index>      which surfaces use a texture
```

For "what is that thing on screen and what draws it".

## A question no command answers

Write fifteen lines of Python, get the answer, throw them away. Copy one of `examples\` in the CgrPy folder
(`writers.py` is the model) into a scratch file outside this project and run it with `cgrpy <path to it>`:

```python
from cgr.cache import load_project
from cgr.decomp import Decompiler

P = load_project()                       # the snapshot build_proj made
g = P.find_group("SpecialPurpose")       # short names work, like everywhere else
D = Decompiler(P, g, max_depth=3)

for c in g.channels.values():            # c.index, c.name, c.type_name, c.value
    if c.name and c.name.startswith("Do_") and "Eraser" in c.name:
        print(c.index, c.name)
        print(D.value(c.index))
```

`g.ports(c)` gives a channel's children by port; `g.by_name[name]` every channel of a name. Scratch scripts are
not part of the project and are not committed.

## How to tell a finding from a guess

- **A name is a lead, never evidence.** Half the graph is named after something it no longer does or after
  nothing (`FindJollyRoger`). What a channel *is*, is who writes it, who reads it, and what the running game does.
- **The game's words are not the player's.** The board is the *buffer*, Eraser's erase is *shatter*, Clean Match
  is the *Butler*, Mono is *Ninja*, Pointman Pro is plain `Do_Pointman`. Translate through the tables in
  `.tweaker/docs/internals/playing-audiosurf.md`, then search.
- **Names repeat inside a group.** The first match is often the wrong one. Every index you keep, write down why.
- **Values in the file are the author's desk.** `query value` shows what a channel computes from the file; a
  plain stored value is what it held when the game was saved in the editor, not what it holds in a run.
- **Group parameters are inputs.** Read inside a hook they hold the last value passed, not the current one —
  find what the handler writes instead.
- **Trace before writing or muting.** Parents and writers, several levels both ways. Every past attempt that
  skipped this changed the wrong thing and looked like it worked.
- **The docs may be behind the game.** Where a doc and `cgrpy` disagree, `cgrpy` on this game wins — and the
  game, seen by a probe script, wins over both.
- **Not verified until seen in the game.** A finding becomes a fact when a probe script showed it (skill
  `audiosurf-in-game-loop`, pattern 9 of skill `tweaker-lua-api`).

## Tools for the binaries

`chtypes`, `vtdump`, `findvt`, `symbols`, `disasm` read the game's DLLs. They answer questions about native code,
which a script cannot call anyway; if they are needed at all, skill `audiosurf-binaries` says how — and if that
skill is not installed, the question is probably out of scope for a script.

## `meshes` — not for this project

`cgrpy meshes` exports and replaces 3D models in `.cgr` files. Replacing writes a game file, which a script
project never does (skill `audiosurf-game-files`). Do not use `meshes replace` here.
