---
name: audiosurf-binaries
description: Questions only Audiosurf's native code can answer - what a channel type's DLL does, which vtable slot is which, a calling convention, a class's parent, whether a documented claim about the engine is true. Finds a decompiler the agent already has (IDA Pro MCP, Ghidra MCP, a user skill) or explains to the user what to set up; falls back to CgrPy's disasm/vtdump/symbols/findvt. Use when the graph (skill cgrpy) and the docs are not enough, or a finding about the engine looks doubtful.
---

# The game's binaries

Most script work never needs this: the graph (skill `cgrpy`) says what the game does, the API says what a script
can do. The binaries matter when the question is about the **engine** — what a channel type does natively, why a
call behaves oddly, whether something the docs claim about the ABI is true — or when a finding is doubtful and
paper is not enough.

**A script cannot call native code.** `ffi` is removed from the script environment; nothing found here turns into
a few lines of Lua. Findings here explain behaviour, decide whether something is possible, or become a request to
the Tweaker's developers (a GitHub issue at github.com/Unchpokable/Audiosurf-Tweaker) for an API the plugin would
have to add.

The map is `.tweaker/docs/internals/channel-abi.md`: which module is what, channel families and their vtable
slots, the object layout, calling conventions, and the ICF trap. Read it first.

## Only on a copy

All of this happens on **a copy of the game the user made for the purpose**, never on the install they play from
(skill `audiosurf-game-files`). Decompiler databases (IDA's `.i64`, Ghidra's project) are written next to the
binaries **of the copy**. Nothing from the game — binaries, databases, decompiled code, dumps — is committed,
uploaded or shared; quoting an address or a function name in a note is fine. If the user has no copy, say that one
is needed and why, and stop.

## Tools, in this order

1. **A decompiler you already have.** Look at what your environment offers before anything else: MCP servers or
   tools for IDA Pro (e.g. `ida-pro-mcp`, often with tools like `decompile`, `xrefs_to`, `get_bytes`), Ghidra
   (e.g. GhidraMCP), Binary Ninja, and skills the user installed for reverse engineering. Use the strongest one
   present. If the user mentioned a tool, ask how it is reached.
2. **None: explain, do not install.** Tell the user what would help and let them decide — you never install it:
   - **IDA Pro** with an MCP bridge (`ida-pro-mcp`) — the strongest option, and paid; what the free edition
     allows (decompiler, plugins) changes between versions, so check before suggesting it;
   - **Ghidra** with an MCP bridge (GhidraMCP) — free and open source, a weaker decompiler, needs a Java runtime;
   - any decompiler they already own, used by hand: you say what to look at, they paste back the output.
   Point them to each project's own install instructions, and offer to help prepare (which binary to open, which
   addresses) — not to run installers.
3. **Plan C: CgrPy's binary tools.** No decompiler, but often enough:

| Question | Command |
|---|---|
| which DLL implements a channel type | `cgrpy chtypes "Array V"` |
| a class's vtable, slot by slot, named from exports | `cgrpy vtdump --type "Array Vector"` |
| a vtable nothing exports, by its shape | `cgrpy findvt --type "Array Unique" --min 23 --ret 20:4 --ret 22:0xc` |
| a module's exports and imports (a parent class shows as an imported constructor) | `cgrpy symbols --type "Array Vector" --imports` |
| what a function does, calls named | `cgrpy disasm --type "Array Vector" 0x1240` |

## Where things are

- `engine\QuestViewer.exe` — the player that runs the project; `engine\HighPoly.dll` — the engine core: the frame
  loop, channel groups, the base channel class.
- `engine\channels\*.dll` — one module per channel type, named by GUID; `engine\channels.lst` maps GUID to type
  name (`cgrpy chtypes` reads it). The Tweaker's own `TweakerPlugin.dll` sits there too — not the game's.
- All x86 (32-bit), MSVC-built; `__thiscall` methods, `ret N` tells the stack bytes.

## Recipes

- **What does slot N do for this channel type?** `chtypes` → the DLL; `vtdump --type` → the slots named from
  exports; the family table in `channel-abi.md` says what each slot means for that family. A slot means different
  things in different families — never carry one family's slot meaning to another.
- **Is this an ABI question the docs answer wrongly?** Disassemble the actual method: the number of stack
  arguments is in `ret N`, `this` is in `ecx`. Trust the instruction stream over any header — the Quest3D SDK
  headers are known to be wrong past offset `0x70` of the object.
- **Who is this class's parent?** `symbols --imports`: a channel DLL imports its base's constructor.
- **One address, several names.** The linker folds identical functions (ICF): a decompiler shows one name, often
  of an unrelated class. Do not conclude "this is X's method" from the name at an address; check the vtable it is
  reached through.

## Paper against the disassembler

The docs and the reverse-engineering journals were written from this game, by people, and can be wrong or behind.
**When a finding is doubtful — it contradicts a doc, explains too much, or rests on a name — stop trusting paper
and open the code.** Say what you checked and how (the function, the address, the tool), so the user can see it is
a finding and not a recollection. When the code and a doc disagree, the code of this game wins; tell the user, and
suggest reporting the doc.
