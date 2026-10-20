# Channels in memory: the native ABI

[← back to the index](../internals.md)

This page is for work on the game's **binaries** — reading them in a disassembler or decompiler, calling into
them, hooking them. It is not needed to write scripts: Audiosurf Tweaker's script layer does all of this for you,
and checks every call. It is here because the moment you go below the graph, every number on this page is the
difference between a working tool and a game that crashes three frames later for no visible reason.

Everything below is the **Steam release** of the game. Addresses are RVAs (offsets from the module's image base);
the image base of `HighPoly.dll` and of every channel DLL is `0x10000000`. All modules are 32-bit x86, built with
Visual Studio 2003.

## Who is who

| Module | Role |
|---|---|
| `engine\HighPoly.dll` | **the engine core**: `A3d_Channel`, `A3d_ChannelGroup`, `EngineInterface`, `EngineControl`, lists, the `.cgr` loader |
| `engine\QuestViewer.exe` | the window and the message loop; calls the engine once per frame. It does **not** import `HighPoly.dll`: it loads it by hand and calls the exported `GetEngineControlInstance` |
| `engine\channels\<GUID>.dll` | 242 DLLs implementing 226 channel types (`engine\channels.lst` maps type names to files). **Everything a channel type means lives here** |
| `Audiosurf.exe` | the launcher |

The core knows no channel type. All it does is call virtual methods — so any hook at core level is either
global, or aimed at one object's vtable.

**Almost everything is exported by name.** Channel DLLs export their vtables (`??_7<Class>@@6B@`) and each
virtual method under its decorated name; `HighPoly.dll` exports the engine's methods. You rarely need a pattern
scan: `cgrpy symbols`, `cgrpy vtdump`, or `GetProcAddress`.

## The vtable: the base and its families

Every channel is an `A3d_Channel`, single inheritance all the way down, one vptr at `+0x00` (in the heap, so
writable; the vtable itself is in `.rdata`, read-only). The base has **17 slots**:

| Slot | Offset | Method |
|---|---|---|
| 0 | `+0x00` | scalar deleting destructor |
| **1** | **`+0x04`** | **`CallChannel`** — the action path |
| 2 | `+0x08` | `OneTimeInitialize` |
| 3 | `+0x0c` | `DoEvent` |
| 4 | `+0x10` | `GetChannelType` |
| 5 | `+0x14` | `SaveChannel` |
| 6 | `+0x18` | `LoadChannel` |
| 7–16 | `+0x1c`…`+0x40` | `SetChannelInterfaceType`, `GetChannelFromChannel`, `Get/SetTimeStamp`, `DoDependencyInit`, `CheckIndexNrRemoval`, `GetInternalChild`, `CheckPublicChannelInstanceRemoval`, `AddMetaData`, `SetChannelGroup` |

The graph is executed through two slots and nothing else: **slot 1** (`CallChannel`) when a channel is called,
**slot 17** when it is read. The base `A3d_Channel::CallChannel` is a single `ret` — folded by the linker with
four other empty functions — and 159 of the 226 types override it, covering 83 % of all channels. A detour on the
base method therefore sees almost nothing that matters.

**From slot 17 on, every family lays out its own methods**, so the same slot means different things:

| Family (base class) | slot 17 `+0x44` | slot 18 `+0x48` | slot 19 `+0x4c` |
|---|---|---|---|
| numbers, `Aco_FloatChannel` | `GetFloat` → float | `GetOldFloat` | `SetFloat(float)` |
| text, `Aco_StringChannel` | `GetString` → `const char*` | `SetString(const char*)` | `SetSingeLine(bool)` |
| vectors, `Aco_VectorChannel` | `GetVector` → `D3DXVECTOR3` | `SetVector(D3DXVECTOR3)` | **`SetFloat(int, float)`** |
| matrices, `Aco_MatrixChannel` | `GetMatrix` → `D3DXMATRIX` | `GetOldMatrix` (same address) | `SetMatrix(D3DXMATRIX)` |
| `Aco_DX8_D3DDeviceUse` | `InvalidateDeviceObjects` | `InvalidateDirectGraphics` | *(end of table)* |
| textures, `Aco_DX8_Texture` | `InvalidateDeviceObjects` | — | **`Release`** |
| geometry, `Aco_DX8_ObjectDataChannel` | `InvalidateDeviceObjects` | — | **`Release`** |
| `3D Object`, `Aco_DX8_ObjectChannel` | **`DrawSurfaces`** | `DrawTransparentSurfaces` | `GetObjectMatrix` → `D3DXMATRIX` |

The numeric family goes on: 20 `GetDefaultFloat`, 21 `SetDefaultFloat`, 22/23 the default-setting pair. The text
family has 13 slots of its own (to 29): 23 `GetIfUseWChar`, 24 `GetWString` — a text channel holds **either** a
narrow or a wide string, and returns an empty or stale one of the other kind without complaint; ask which first.

### Why the family must be checked before every call

Calling slot 17 or 19 on a channel of the wrong family is not "a wrong number". It is:

- `GetFloat` on a `Texture` or `3D ObjectData` — `InvalidateDeviceObjects()`: the game loses a device resource;
- `GetFloat` on a `3D Object` — `DrawSurfaces()`: the object is **drawn** in the middle of someone else's frame;
- `SetFloat(v)` on a `Texture` or `3D ObjectData` — `Release()`, with a stray argument on the stack;
- `SetFloat(v)` on a `3D Object` — `GetObjectMatrix()`, which takes `v` for a pointer to a 64-byte buffer and
  **writes there**;
- `SetFloat(v)` on a `Value Vector` — a stack imbalance (`SetFloat(int, float)` pops one more argument);
- slot 17 on a type that added no methods of its own — past the end of its vtable: the next bytes of `.rdata`,
  called as a function.

None of these looks like an error when it happens.

**The check:** `GetChannelType` (slot 4, present on every channel) returns a 132-byte `ChannelType` — name at
`+0x00`, the type's `guid` at `+0x50`, its **`baseguid`** at `+0x60`. The base GUID names the family:

| `baseguid` | Family | Types |
|---|---|---|
| `BE69CCC4-CFC1-4362-AC81-767D199BBFC3` (`Value`) | numbers — has `GetFloat`/`SetFloat` | 30 |
| `6E6FB247-…` (`Text`) | text | 19 |
| `21A8923D-…` (`3D ObjectData`) | geometry | 15 |
| `2F605354-…` (`Matrix`) | matrices | 8 |
| `BC052C38-…` (`Texture`) | textures | 6 |
| `9D045960-…` (`Value Vector`) | vectors | 4 |

The numeric family includes `Value`, `Expression Value`, `ValueOperator`, `Set Value`, `Trigger`, `Envelope`,
`Array Value`, `Lua Script`, `UserInput`, `TickCount` and more. Check once, when you resolve the channel; never on
a per-frame path. The cheaper way to find a type's family offline: **a channel DLL imports its base class's
constructor from the base's DLL** (`cgrpy symbols --type "<type>" --imports`).

### Calling conventions

- Methods are `__thiscall` (`this` in `ecx`); a method cleans its own arguments, so `ret 4` means one dword.
- Structures returned by value (`GetVector`, `GetMatrix`, `GetChannelType`) come back through a **hidden pointer
  to the destination, passed as the first stack argument**.
- `SetVector(D3DXVECTOR3)` takes the vector **by value**: three dwords from `[esp+4]`, `ret 0xc` — identical, on
  the stack, to three separate `float` arguments.
- **`SetVector` does more than store.** After writing its own value it walks children 0–2 and, for each that is a
  numeric channel, calls its `SetFloat` with the matching component. Writing a vector pushes into the numbers wired
  to it.

### Two ways to call a method, and when each is right

| | Through the object's vtable (`vt[+0x44]`) | Through the DLL's export, with an explicit `this` |
|---|---|---|
| Runs | the implementation of the object's **actual** type | **exactly** the implementation you named |
| Right when | the type is not known in advance | the type is known for certain |

The exported `Aco_FloatChannel::GetFloat` called on an `Expression Value` runs the *base* logic ("return child 0")
instead of the formula: no crash, a silently wrong number. If you accept arbitrary channels, go through the vtable.

## Tables

`Array Value` (36 slots, a numeric channel) and `Array Vector` (28 slots, a vector channel) **override their
family's setter** — slot 19 `SetFloat(float)` and slot 18 `SetVector` respectively — so a write goes into the
current row of the table, not into the channel's own scalar. The row is taken from child 0's value **at the moment
of the call**, truncated to int — the same cursor the getters use.

Three traps:

- **The write is not memoised and is always applied — and so is the copy.** Both setters also store the value in
  the channel's own scalar (`+0x7c`), and a memoised getter returns that scalar. A probe that writes, reads back
  in the same frame and compares will always say "it worked", even when the table write was skipped. Verify with
  memoisation bypassed, or on the next frame, or through another reader.
- **Failure is silent.** If the table is not bound, `Aco_Array_Value::SetFloat` quietly writes only the scalar;
  `SetVector` tries to reconnect and gives up the same way. Both return `void`.
- **There are no bounds, and that is worse than no bounds.** The table object (`ArrayConnectItem`, implemented in
  the `Array Unique` DLL, exported by nothing) **creates a missing row** on write: a write past the end
  **lengthens the game's table**. Check the row first — its slot 18 `GetRow(int)` returns NULL without creating,
  slot 13 `GetRowCount` gives the bound. Slots 14–19 also hold `AddRow`, `InsertRow`, `RemoveRow`: one slot off
  and you are editing the table's shape.

## The channel object

The Quest3D SDK headers match the shipped binary **only up to `+0x70`**; after that they describe a later engine
revision. Never cast a channel to an SDK class and read its fields — `Aco_FloatChannel::channelFloat_` is at
`+0x74` by the header and at **`+0x7c`** in the game. Eight bytes off, no crash, just garbage.

| Offset | Field |
|---|---|
| `+0x00` | vptr |
| `+0x08` | `EngineInterface*` |
| `+0x0c` | cached `ChannelType*` |
| **`+0x10`** | **the frame the channel was last computed in** (`channelCalculatedAtCount_`) |
| `+0x14` | index within the group |
| `+0x18` | interface type (`ACO_*`, e.g. parameter) |
| `+0x24` | input channel list (`A3d_List`, 16 bytes) |
| **`+0x50`** | **name** (`char*`) — `GetChannelName` is literally `mov eax,[ecx+0x50]; ret` |
| `+0x54` | the channel's group |
| **`+0x60`** | **`CHIC`**: 1 = never memoised |
| `+0x64` | time stamp |
| `+0x7c` | the numeric family's value (every `Aco_FloatChannel` child, `Aco_Lua` included); also the vector's storage |
| `+0x88` | `sizeof(Aco_FloatChannel)`; in `Aco_Lua`, the `lua_State*` |

Memoisation in full (`A3d_Channel::CheckRenderCount`):

```c
if (this->ignoreTreeCount /* +0x60, CHIC */) return true;          // always recompute
int now = group ? group->GetTreeCalculateCount() : engine->GetTreeCalculateCount();
if (this->calculatedAt /* +0x10 */ != now) { this->calculatedAt = now; return true; }
return false;                                                       // already computed this frame
```

So for a `CHIC = 0` channel, `+0x10 == group->GetTreeCalculateCount()` means "computed this frame" — the test the
[lifecycle](lifecycle.md) page relies on.

## The frame, in code

```c
// EngineControl::EngineLoop, HighPoly.dll RVA 0x10b10
if (this->inLoop || !this->running) return;
this->inLoop = 1;
ext->IncreaseTreeCount();                                   // once per frame, always
A3d_ChannelGroup* g = engine->GetChannelGroup(ext->GetStartGroup());
if (g) g->CallStartChannel();                               // the start group only
this->inLoop = 0;
```

`EngineLoop` is the right place for anything that must run once per frame on the engine's thread, before or after
the graph. `EngineControl::GetEngineInterface()` (slot 5) gives the `EngineInterface*` from the first frame.

Useful exports of `HighPoly.dll`:

```
?EngineLoop@EngineControl@@UAEXXZ                         the frame
?GetTreeCalculateCount@EngineInterface@@UAEHXZ            current frame number (wraps at 30000)
?GetStartGroup@EngineInterfaceExt@@UAEHXZ                 index of the start group
?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@H@Z         group by index
?GetChannelGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@PBDH@Z      group by file name
?GetPoolGroup@EngineInterface@@UAEPAVA3d_ChannelGroup@@PBDH@Z         group by pool name
?GetChannelGroupCount@EngineInterface@@UAEHXZ             how many groups now
?GetChannelGroupFileName@A3d_ChannelGroup@@UAEPBDXZ
?GetPoolName@A3d_ChannelGroup@@UAEPBDXZ
?Release@A3d_ChannelGroup@@UAEXXZ                         every group's destruction passes here
?DeleteChannelGroup@EngineInterface@@UAEXH@Z              the Remove Group path
```

Do **not** call `GetEngineControlInstance` (it is a factory: it creates a second, empty engine) or
`GetQ3DStartGroup` (it loads the group from disk if missing, and shows a message box).

### Cost

`A3d_ChannelGroup::GetChannel(const char*)` is a linear scan with `_stricmp` over every channel of the group —
about 7 000 comparisons in `XX_StartHere`. Resolve channels by name once, keep the pointer (for as long as its group
lives — see [lifecycle](lifecycle.md#groups-come-and-go-all-session)), and never look one up per frame. A
`GetFloat` through the vtable is cheap: one indirect call plus the memoisation check.

### Dead ends that look alive

- **`EngineListener` / `AboutToReleaseChannel`** — a "channel is being released" notification the SDK promises.
  Registering works; nothing in the shipped game ever calls it. Invalidate on group release instead.
- **`A3d_Channel::CallChannel` as an event stream** — it is the empty base; see above.

## Reading the binaries: tools and one trap

`cgrpy` answers most questions here without a disassembler: `chtypes` (type → DLL), `vtdump` (a vtable named slot
by slot), `findvt` (vtables nothing exports, found by shape), `symbols` (exports and imports), `disasm` (a function,
calls named). For decompilation use IDA or Ghidra on **a copy of the game**: both write their databases next to the
binary.

**One address can have several names.** The linker folds identical short functions into one (`/OPT:ICF`), and then
several exports — sometimes of different classes — point at the same RVA: `GetMatrix`/`GetOldMatrix` in the matrix
channel, `EngineControl::GetEngineInterface` and `A3d_String::GetString` in `HighPoly.dll`. `vtdump` prints every
name; IDA shows **one** of them, so a slot in its listing can carry another class's name. For hooking it is not
cosmetic: a detour on such an address catches everyone who shares it.

## How to check any of this

```
cgrpy chtypes "Array V"                              the two table types and their DLLs
cgrpy vtdump --type "Array Vector"                   SetVector in slot 18
cgrpy symbols --type "Array Vector" --imports        its parent: Aco_VectorChannel
cgrpy disasm --type "Array Vector" 0x1240            SetVector: three dwords, ret 0xc
cgrpy findvt --type "Array Unique" --min 23 --ret 20:4 --ret 22:0xc     ArrayConnectItem
cgrpy vtdump HighPoly.dll                            EngineControl, EngineInterfaceExt, ...
```
