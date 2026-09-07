# CgrPy

Tools for reading Audiosurf from the outside: its Quest3D channel graphs (`.cgr`) and the DLLs that
implement them.

Almost everything in `Docs/Internal/reversing-journal-*.md` was found with these. When a journal
entry quotes a channel index, a vtable slot or an RVA, this is what produced it — and it is what you
re-run when you need to check whether a claim in there is still true.

## Why it lives here

It used to live in `Temp/`, which is gitignored and periodically deleted. Its own README said the
docs were self-contained enough to rewrite it from scratch, which was true in the sense that nothing
would be *lost* — and false in the sense that anyone needing it would first have to spend an evening
rebuilding a `.cgr` parser before they could ask their actual question.

Two things follow from moving it, and both are the point:

- **it survives a cleanup**, and
- **it is documented**, so the next person to open it does not start by reading the parser.

What is *not* here, and never will be, is the game. `Audiosurf_Steam/` is gitignored: these tools
read someone else's files, they do not ship them.

## Setup

You need [uv](https://docs.astral.sh/uv/) on PATH and an Audiosurf install. Nothing else — uv builds
the virtualenv and installs dependencies the first time you run something.

```
uvrun                       # list the tools
uvrun vtdump --type "Array Vector"
```

The game is located automatically: `AUDIOSURF_DIR` if you set it, else `<repo>/Audiosurf_Steam`, else
the usual Steam paths. Set the environment variable if yours is somewhere else — no tool has a path
hardcoded in it, and none should ever get one back.

## Two families of tool

They are worth keeping apart in your head, because one needs a slow setup step and the other does
not.

**Binary tools** (`vtdump`, `findvt`, `disasm`, `symbols`, `chtypes`) read a DLL or EXE directly.
Nothing to prepare, instant, and they answer questions about *code*: what slot is a method in, what
does it derive from, what does this function actually do.

**Graph tools** (`render`, `meshes`, plus everything in `examples/`) answer questions about *data*:
which channel feeds which, who calls this, where does this colour come from. They read a parsed
snapshot of all 161 groups, so run this once first:

```
uvrun build_proj
```

It writes `.cache/proj.pkl` (~200 MB, gitignored, under a minute to build). `meshes` is the exception
— it reads `.cgr` files directly, which is why it also works on the two groups whose channel framing
is damaged.

### Read what build_proj prints

```
161 groups, 131356 channels, 0 unparsed bytes, 0 errors
```

**`unparsed bytes` is a correctness check, not a statistic.** Chunk walking is a framing problem: if
the walker loses sync it does not stop, it carries on emitting plausible nonsense. Nonzero means some
region was skipped and anything derived from that group is suspect.

The measured baseline is in `rdy2use/build_proj.py`'s docstring, including which groups are expected
to be imperfect and why. Treat a regression against it as a parser bug rather than a fact about the
game.

## What's in rdy2use

| Tool | What it answers | Where it was used, and why it helped |
|---|---|---|
| `build_proj` | — (builds the cache the graph tools need) | Every graph question below starts here. |
| `chtypes` | Which DLL implements a channel type | `Array Value` → `DF5BF7F7-…`, `Array Vector` → `DD626E09-…`. A type name means nothing to a disassembler; this is the first step of every binary question about a channel. |
| `vtdump` | What is in a class's vtable, slot by slot, with names | **Settled whether scripts can write a table row at all.** `Aco_Array_Value` and `Aco_Array_Vector` each override the setter their family already had (slot 19 and slot 18). Had they inherited it, a write would have gone into the channel's own scalar and the table would never have moved. Engine journal §2.2.3. |
| `findvt` | Vtables the module does not export, found by shape | Found `ArrayConnectItem`, which is exported by nothing. The call sites showed slot 20 taking one argument and slot 22 taking three; `--min 23 --ret 20:4 --ret 22:0xc` returned exactly one candidate. That table holds `GetRow` and `GetRowOrCreate` one slot apart — and telling them apart is the difference between a bounds check and silently growing the game's tables. |
| `disasm` | What a function actually does, with calls named | Read `Aco_Array_Vector::SetVector` to answer two things at once: where the row index comes from (child 0, truncated to int) and whether `D3DXVECTOR3` arrives by value (three dwords from `[esp+4]`). The second cannot be settled by reasoning about the ABI — guess wrong and it is a stack imbalance, not a wrong number. Engine journal §2.2.2. |
| `symbols` | A module's exports and imports | **Imports are the cheap way to find a C++ parent class:** a channel DLL imports its base's constructor from the base's own DLL. `Aco_Array_Vector` importing `??0Aco_VectorChannel@@…` is how its family was confirmed, and with it which slots it inherits. |
| `render` | The draw path of a scene node, up and down | `find` turns a name into indices, `chain` walks 3D Object → Surface → geometry/material/texture, `callers` walks upwards to what actually invokes a node each frame. Both walks follow cross-group import stubs. Most of the render journal's §5 came out of these. Worked example: `find … GridColor` returns **three** channels of that name, and `callers` on #202 lands on Material `ro_01` under Surface `ro_01 1` — road furniture, not the puzzle tiles it was assumed to colour. |
| `meshes` | Geometry: inventory, export to OBJ, splice back | 930 mesh blobs inventoried; `verify` is the round-trip baseline (893 re-encode byte-exact, 930 OBJ round-trip, 0 problems). Geometry journal. `N` is a **stream ordinal**, not a channel index — one channel can hold several blobs. |

Every tool prints its own usage, with the same "where it was used" note, when run with no arguments.

## Gotchas

**The graph tools take a channel index, not a name.** `render find` is how you get one, and it
prints *every* match rather than the first: names are not unique within a group, and a tool that
silently resolved one would pick the wrong channel exactly when it matters. Whenever you record an
index somewhere, record why that one.

**`=` and `,` are argument delimiters in cmd.** `--ret 20=4,22=0xc` arrives shredded. Use the colon
form, one flag per slot: `--ret 20:4 --ret 22:0xc`.

**A run of code pointers is not one vtable.** MSVC lays them back to back in `.rdata` with nothing
between, so `findvt` reports a run that can span several classes and `vtdump` can walk past the end of
the one you asked for. The real boundary shows up in the names — where the class prefix changes — or
as the type's own name appearing as a string. Reading the run length as the table length is how a
class acquires methods belonging to its neighbour.

**`proj.pkl` is a cache, not an artifact.** It is derived entirely from the player's own install.
Never commit it; rebuild it instead.

**A clean parse is not a complete parse.** See build_proj's docstring: 17 asset-heavy groups lose
trailing non-channel chunks, and two do not reconcile their channel count. Every group the journals
analyse is clean, but check before trusting a new one.

## examples/

Ad-hoc queries, kept because the pattern is the point rather than the answers. The normal working
mode with the graph tools is not "run a tool" but "write fifteen lines against `cgr.project` and
`cgr.decomp`, get an answer, throw it away". These three are what that looks like:

- `writers.py` — who writes each run statistic, by scanning every `Set Value` in a group and looking
  at its target port. The shape most graph questions reduce to.
- `qchic.py` — reading a chunk field (`CHIC`, the per-frame memoisation flag) straight off the stream
  rather than through the parsed graph, because the parser does not keep it.
- `genall.py` — a bulk artifact generator: dumps decompiled pseudocode and CSVs for a set of groups
  at once.

They are examples, not maintained tools: they hardcode the groups they were written for.

## The package

| Module | Role |
|---|---|
| `cgr/core.py` | container: zlib + XOR 0x04 peel, chunk walk, `channels.lst` |
| `cgr/graph.py` | graph parser with lookahead framing (bare `A3DG`/`CHES`/`STNW`) |
| `cgr/project.py` | all groups, `CHES` import resolution, `group_params` |
| `cgr/decomp.py` | graph → pseudocode |
| `cgr/mesh.py` | `3D ObjectData` geometry: chunks ↔ arrays, OBJ ↔ mesh |
| `cgr/container.py` | lossless unwrap/rewrap of the zlib + XOR envelope |
| `cgr/patch.py` | locate mesh blobs by stream ordinal, splice replacements |
| `cgr/pe.py` | minimal PE reader: RVA↔offset, exports, imports, vtables |
| `cgr/chtypes.py` | `channels.lst` → type name → DLL |
| `cgr/paths.py` | where the game is; the one place that knows |

The import name is `cgr`, not `cgrpy` — every script says `from cgr.project import ...`, and the
directory is named to match.

`pe.py` is deliberately hand-rolled rather than built on `pefile`. These files are read next to a
disassembler and a set of notes that quote raw RVAs, and every layer that renames or normalises
something is a layer where an address in the journal stops matching an address on screen.

## Where the format is written down

This directory is the tooling; the findings are in the journals:

- `Docs/Internal/reversing-journal-gameplay.md` — `.cgr` format (§1), node semantics (§2)
- `Docs/Internal/reversing-journal-engine.md` — the native side: channel ABI, vtables, object layout
- `Docs/Internal/reversing-journal-geometry.md` — meshes, chunks, OBJ round-trip
- `Docs/Internal/reversing-journal-render.md` — the track's visual environment
- `Docs/Internal/reversing-journal-lua.md` — the game's own Lua engine

If a tool here and a journal there disagree, the journal is the claim and the tool is the evidence —
re-run it before believing either.
