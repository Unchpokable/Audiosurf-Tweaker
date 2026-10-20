# How Audiosurf works inside

These pages describe the game itself — not Audiosurf Tweaker. How its files are built, how its engine runs a
frame, how its logic is laid out and what each part of it means to the person playing. They are for anyone who
wants to change or read the game: script authors who need a channel the [scripting manual](scripting.md) does
not name, modders, and AI coding agents working for either.

Everything here was recovered by reverse engineering. Audiosurf was built in **Quest3D**, a visual engine, and
its author never published a modding interface; what is written down is what was found, checked, and found
again.

## The pages

| | |
|---|---|
| **[Playing Audiosurf](internals/playing-audiosurf.md)** | The game from the player's seat: screens, characters, abilities, the board, scoring, the HUD — every term tied to the groups and channels that implement it. **Start here**, even if you have played for years: the game's words and the graph's words are not the same. |
| **[The channel graph and the `.cgr` format](internals/graph-format.md)** | What a `.cgr` file is, what a channel and a group are, how channels are wired, how groups call each other, what each common channel type does. |
| **[Start-up and the life of groups](internals/lifecycle.md)** | How the engine runs a frame, in which order the game loads, why a channel read too early lies, and how groups are unloaded and loaded again while you play. |
| **[Gameplay map](internals/gameplay-map.md)** | The groups that matter for scripts and their channels: the run's statistics, scoring, medals, Skill Rating, the colour palette, the board, the chain, the HUD. With indices, and with the traps. |
| **[Channels in memory: the native ABI](internals/channel-abi.md)** | For work on the game's binaries: which DLL is what, channel families and their vtable slots, the object layout, what goes wrong when a slot is called on the wrong family. |

## How these pages relate to everything else

- **The [scripting manual](scripting.md)** is how to *use* the game from a Lua script. These pages are the
  map it points to when you need something it does not cover.
- **The tools** that found all this are shipped with Audiosurf Tweaker as `DevTools\CgrPy` and described in
  the [CgrPy README](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Tools/CgrPy/README.md). Every
  claim here that carries a channel index or a number can be checked with them — `cgrpy query` most of the
  time — and that is worth doing before building anything on it.
- **The reverse-engineering journals** in the repository are the source these pages are written from: the
  full working notes, in Russian, with the history of every finding and the commands that produced it. Where a
  page here and a journal disagree, the journal is the claim of record — and the game, checked with CgrPy, is
  the judge of both.

| Journal | Feeds |
|---|---|
| [gameplay](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-gameplay.md) | graph format, gameplay map |
| [boot](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-boot.md) | start-up and groups, channel families |
| [engine](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-engine.md) | native ABI, how a frame runs |
| [lua](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-lua.md) | the `.cgr` container, the game's own Lua |
| [geometry](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-geometry.md) | meshes (not written up here yet) |
| [render](https://github.com/Unchpokable/Audiosurf-Tweaker/blob/HEAD/Docs/Internal/reversing-journal-render.md) | the track's visuals (not written up here yet) |

Rendering and audio are not covered by these pages yet: so far only the Tweaker itself works with them.

## Three rules that run through all of it

1. **A name is a lead, never evidence.** Many channels are named after something they no longer do, after
   nothing at all, or the same as another channel in the same group. What a channel *is* is who writes it, who
   reads it, and what the running game shows when you watch it.
2. **A value read before the engine has computed it is the author's desk, not the game.** Every channel in a
   `.cgr` file carries whatever it held when the game's author last saved the project. Until the graph runs,
   reading it gives a plausible, wrong number ([lifecycle](internals/lifecycle.md)).
3. **Leave the game's files alone.** Read them; work on copies; never ship or share them. Audiosurf Tweaker
   changes the running game from the outside and touches its install only in `engine\TweakerStuff\` and the
   plugin's own DLL.
