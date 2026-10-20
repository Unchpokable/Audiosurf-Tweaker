# The channel graph and the `.cgr` format

[← back to the index](../internals.md)

Audiosurf has no `main()`. Its logic, its rendering and its menus are one **graph of channels**, built in
Quest3D's visual editor and stored as `.cgr` files under the game's `engine\` folder. This page is the shape of
that graph and of the files that hold it — enough to read either. [How Audiosurf is built](../scripting/game-model.md)
in the scripting manual tells the same story from the script author's side and is the gentler start.

## Groups and channels

- A **channel** is a node: a value, a formula, an action, a texture, a mesh, a table. It has a **type**, a
  **name**, an **index** within its group, and **ports** — numbered inputs, each holding an ordered list of
  child channels.
- A **group** is one `.cgr` file: a namespace and a unit of loading. The Steam release has about 160 of them
  and some 131 000 channels. Groups have a file path (`Scores/StatCollector.cgr`) and, once loaded, a **pool
  name** (`StatCollector`); the root group `XX_StartHere.cgr` is the pool **`StartGroup`**.
- Groups reference each other by **name**: a channel in one group can stand for a channel of another
  (an *import*), and that is how the whole game hangs together.

**Names are not unique** inside a group — `TrafficCommander` has two `Value`s called `TrafficType` (#631 and
#900), and the game's own lookup by name returns the first. Where a name is ambiguous, address by index.

## The file, layer by layer

A `.cgr` is a stream of chunks, `TAG` (4 ASCII bytes) + `u32 length` + payload, wrapped twice:

```
level 0, plain:       ACTF  16  GUID of the ZipCompression channel type (7DFC389A-...)
                      ZIOS   4  unpacked size
                      ZINS   4  packed size
                      ZICB   N  zlib stream
level 1, after zlib:  ACTF / NECL / NECT ("Protected files") / NEOS
                      NECB   N  payload, XOR 0x04 - tags, lengths and strings alike
level 2, after XOR:   the channel graph, starting with QVRS (Quest3D VeRSion)
```

Beside `Scores/StatCollector.cgr` the game ships `unprotected - StatsCollector.cgr`: the same group (same GUID),
left behind unencrypted by the author — a ready test case for any parser.

Inside level 2 three things trip a naive reader, and each one loses data silently:

- **Not every tag has a length.** `A3DG` (start of the graph), `CHES` (see imports) and `STNW` (a `Text` flag)
  are bare 4-byte markers. `CHES` is also a normal chunk with a length (696 bytes) elsewhere; one token of
  lookahead tells which — if the next 4 bytes are a valid tag, it was the bare marker.
- **The stream is nested.** `DGCG` (a channel group) and `ESCH` (a sub-group) carry a whole chunk stream inside
  their payload. Stepping over payloads skips everything in them.
- **Never resynchronise byte by byte.** A walker that loses its place and searches for the next plausible tag
  finds them inside GUIDs. Read from a known chunk boundary, jump whole payloads, and report a position you
  cannot read instead of guessing. CgrPy's parser does exactly this and counts what it could not read
  (`cgrpy build_proj`).

### A channel record

```
CHIX u32      index within the group
CHID guid16   the channel's TYPE -> engine\channels.lst (type name + DLL)
[CHES]        bare marker: this channel has an external link
CHIC u8       1 = never memoised: recomputed on every read (see "Reading values" below)
CHNA str      name
CHIT u32
[CHES 696]    import record (see below)
CHLC u32      number of link slots
(CHLI i32, CHUL u32, [CHRP u32])*   the links
CHUP u8
<type-specific chunks: FLVA, STVA, ATRS/ATRD, LUSL/LUSC, ...>
```

The links are what make it a graph:

- `CHLI` — the child's index; `-1` is an empty placeholder the editor left;
- **`CHUL` — which input port** of this channel the child is plugged into;
- **`CHRP` — the child's position within that port.**

Read without ports, a channel looks like a flat list of children and the logic is lost. With them, every type
below has a fixed meaning per port.

`engine\channels.lst` maps the 226 channel type GUIDs to names and to their DLLs in `engine\channels\`
(`cgrpy chtypes`).

## What the common types do

| Type | Port 0 | Port 1 | Port 2 | Data |
|---|---|---|---|---|
| `ChannelCaller` | calls, in order | — | — | — |
| `Set Value` / `Set Text` | source (none: use own `FLVA`/`STVA`) | targets | — | `FLVA` / `STVA` |
| `If` | condition | body | — | — |
| `IfElse` | condition | then | else | — |
| `ForLoop` | count | index out | body | — |
| `Expression Value` | operands A, B, C… in slot order | — | — | **`FLVA` is the formula text** |
| `ValueOperator` | operands | — | — | `VEOT` |
| `ChannelSwitch` | selector | one branch per value | — | — |
| `CallSelected` | selector | default branch | one branch per value | — |
| `Array Value` / `Array Vector` | row index | — | — | `TIST` table, `TISC` column |
| `Array Command` | target | — | — | `ACCS` (1 clear column, 7 clear table) |
| `Value` / `Text` | value source | — | — | `FLVA` / `STVA` |
| `Lua Script` | children = `channel.GetChild(i)` | — | — | `LUSL` / `LUSC` (source text) |

- **`ChannelCaller` is the graph's `;`.** Every `Do_Something` handler in the game is one: calling it calls its
  children in order.
- **`Expression Value`'s formula is a string**: `MIN(B,MAX(0,A))`, `(A==2&&(!B))?C*1.2:C` — letters are its
  children in slot order.
- **A named `Value` is a variable, not a constant.** Its `FLVA` is whatever the author's editor last held.
  Only unnamed values are reliably constants.

`cgrpy query dec <group> <channel>` prints a handler in exactly these terms: `X := expr` for a `Set Value`,
`switch` for a `ChannelSwitch`, `for` for a `ForLoop`, `call G::Do_Y(...)` for a call into another group.

## Imports: how groups call each other

A channel carrying a `CHES` record of 696 bytes stands for a channel in another group:

```
+0    char[80]  the channel's name in the source group
+80   char[80]  the source group: a file ("Highway.cgr"), a bare group name ("StatCollector"),
                or "StartGroup" for XX_StartHere.cgr
+160  ...       a path on the author's machine, and leftover memory - ignore
```

Resolution is **by name** inside the named group — the same first-match lookup as within a group.

**Group parameters.** A channel with a bare `CHES` and no record is an *input parameter* of its group. A caller
binds values to them through extra ports on its import stub: port N binds parameter N, parameters ordered by
channel index. `StatCollector` has four, and every call of `Do_UpdatePoints` binds the last two:

| # | Parameter | Bound by |
|---|---|---|
| 0 | `ReportMatch_TotalMatchedBlocks` (#92) | `Do_ReportMatch` |
| 1 | `ReportCarCollected_Color` (#96) | `Do_ReportCarCollected` |
| 2 | `UpdatePoints_Delta` (#154) | `Do_UpdatePoints` |
| 3 | `UpdatePoints_Color` (#155) | `Do_UpdatePoints` |

**A parameter cannot be read from outside.** Its value exists only while the call is being made, through the
caller; read directly, it returns its own stale field. Read the **source** the callers pass in instead — for
`ReportCarCollected_Color` that is `Puzzle::LaneCrashColor`. This is the most convincing wrong answer in the
game: the number looks right and never changes.

Some groups also declare parameters their body never reads (`MoneyFloaterCommander`'s `Launch_*`): callers bind
them, the body reads `in*` channels the caller sets just before. Check `parents` before trusting a parameter.

## Tables

```
ATTG guid16   table GUID
ATTN str      table name ("Stats", "Puzzle_State", ...)
ATCC u32      column count
  per column: ATCN name, ATCW editor width, ATCT type, ATUI column UID, ATRC row count
ATRS/ATRD     rows (only for tables filled in the editor)
```

An `Array Value` or `Array Vector` channel is a **cursor into one column**: its `TIST`/`TISC` name the table and
column, and its port-0 child is the row index. The game reads a table by writing a row number into that child
(conventionally `Index_Something`) and evaluating the column — usually in a `ForLoop`. The game's live tables
are empty in the file (`ATRC = 0`) and filled at run time.

Scripts use `tw.array`, which does that dance and **puts the cursor back**: the cursor is the game's, and leaving
it moved corrupts whatever the game does next ([Reading and writing the game](../scripting/channels.md)).

## Reading values: memoisation

The engine computes a channel at most **once per frame**: the first read computes, every further read in the
frame returns the cached result. Two exceptions matter:

- `Set Value` runs on every call.
- A channel with **`CHIC = 1`** is never memoised and recomputes on every read. About 9 % of all channels carry
  it, and almost every `Array Value` (400 of 431) — a memoised cursor would be useless to the game's own loops.

The consequence for anyone reading the graph from outside: a value read twice in one frame is the same value,
even if something changed in between — unless the channel is `CHIC = 1`. The [native ABI](channel-abi.md) page
has the mechanism.

## The game's own Lua

Quest3D carries **Lua 5.0.2** in the `Lua Script` channel type, and Audiosurf uses it sparingly — where the graph
is awkward: text parsing, search loops (`Puzzle::FindMatches`, the match finder), dynamic loading. Scripts are
stored as **source text** in `LUSC`, so every one of them can be read. Each `Lua Script` channel has its own VM;
scripts talk to the graph only through `channel.GetChild(i)` — so **the order of a Lua channel's children is part
of its contract** — and through `channel.GetPublic(name)`. They call `CallChannel()` when the channel is called
and `GetValue()` when it is read. A runtime error in one is reported once, with the channel's name as the chunk
name. This is the game's Lua, not the Tweaker's: scripts you write run in a different, LuaJIT, environment.

## How to check any of this

```
cgrpy build_proj                                   parse the game once
cgrpy query find "^TrafficType$" --group TrafficCommander     two channels, one name
cgrpy query dec StatCollector Do_UpdatePoints      the scoring funnel as pseudocode
cgrpy query users Do_UpdatePoints                  everything that calls it
cgrpy examples\qchic.py                            the CHIC flag across the project
```
