# Limits, safety and performance

[← back to the index](../scripting.md)

## Where your script runs

Inside the game process, on the game's own thread.

That single fact explains most of the rules below. There is no separate scripting thread, no message
queue, and no isolation. When your `on_frame` handler runs, the game is waiting for it. When your
`on_call` handler runs, the game is *halfway through its own logic* and waiting for you to return.

The upside is that everything is simple: no locks, no races, no stale snapshots. What you read is what
the game has right now, and what you write lands immediately.

The downside is that slow code is a frame rate drop, and only you can prevent it.

## Performance

The scripting layer is built to be cheap — calls from Lua into the plugin are compiled into direct
native calls rather than going through a slow generic bridge, so an individual call costs very
little. Budget is spent on *how many* things you do, not on the fact that you are doing them from
Lua.

**Rules that matter:**

1. **Resolve channel handles once, at the top level.** A by-name lookup is a linear scan over
   thousands of channels. A handle caches the result; one created inside `on_frame` never gets to.

2. **Do not do thousands of things per frame.** Walking a big table is the usual culprit — spread it
   over frames. See
   [Reading and writing the game § Walking a big table](channels.md#walking-a-big-table).

3. **Return early when there is nothing to show.** If your widget is hidden, bail out before reading
   channels and before drawing. The cheapest work is work you skip.

4. **Throttle anything that does not change every frame.** `if tw.frame % 30 == 0 then ... end` is
   fine for re-reading a settings value.

5. **Keep `on_call` handlers short.** They run inside the game's logic, potentially many times a
   second during a run. Accumulate a number; draw it later in `on_frame`.

Nothing here is exotic. The one genuinely counter-intuitive rule is the first one.

## Writing to the game

`handle:set()` works. The engine will not crash from being handed a number.

### Writes are refused until the game has finished loading

This is enforced, not advice: a write attempted while the game is still loading is dropped, and you
get one notification saying so.

The reason is the nastiest failure mode found so far. Writing into the graph while the game is still
assembling itself **does not crash it** — it makes it quietly wrong. Observed symptoms include a
broken track generator and characters swapped around in the menu, produced by a script whose only
stated job was recolouring tiles. Nothing links the symptom to the cause, and a player has no way to
guess that a colour script broke their track.

Scripts injected early are exactly the ones that hit this, because their first frames land in the
middle of the load.

The signal is **"the game has stopped loading things"**, not any particular game state: once the set
of loaded channel groups has been unchanged for a second, writing is allowed for the rest of the
session. That works whether the plugin went in before the game started or into a session already in
progress - injecting mid-run does not require walking back to a menu first.

It is a heuristic and worth treating as one: it says nothing has loaded recently, not that the game
is definitely ready. `tw.can_write()` reports it, so a script can wait deliberately instead of
having writes disappear.

A refused write returns `false`, the same as one that could not resolve — there is no third outcome
for a script to handle.

What the API cannot do is tell you whether the number *makes sense*. It has no idea that the value
you just wrote is one the game will use as a table index, or divide by, or compare against a total it
keeps separately. Writing is the one part of this API where being wrong is not caught.

Practical guidance:

- **A channel with an input wired to it will not hold a write at all** — it is re-derived from that
  input on the next evaluation, and because evaluation is memoised per frame, it does not even fail
  consistently. Only leaves hold. See
  [The rule that decides whether any write survives](channels.md#the-rule-that-decides-whether-any-write-survives).
- **Values the game only reads** (thresholds, cosmetic settings) are usually safe.
- **Values the game also writes** are a race you lose — it overwrites you on the next frame.
- **Table cursors** break whatever the game reads next. Use `tw.array`, which restores them.
- **A table row index outside the table is not a harmless miss.** The engine's write path *creates*
  the missing row, lengthening a table everything else reads. `array:set` refuses such an index for
  you, and `array:rows()` tells you the range — but only writes made through `array:set` get that
  protection.
- **Vector writes reach further than they look** - the engine also writes each component into the
  numeric channel feeding it. See [Writing a vector](channels.md#writing-a-vector).
- **Anything with a plausible valid range** almost certainly has one, and going outside it is on you.

If your goal is to remove behaviour rather than change a number, `tw.mute` is usually the safer tool —
it does not fabricate a value at all.

### Finding the channel is not the hard part

Before writing anything, **dump the channel's parents and children several generations in both
directions** and work out what actually drives it and who actually reads it. Every attempt so far
that skipped this step wrote to the wrong place and looked like a bug in the API:

- A vector that looked like the grid colour turned out to colour road furniture and the hit flash.
- A palette that read back plausible colours turned out to be the *fallback* branch of a three-level
  switch, live only when the player has configured no colours.
- A channel that accepted the write lost it again on alternate frames, because a settings slider was
  wired into it two levels down.

None of those failed loudly. All of them returned sensible-looking numbers.

It is worse than ordinary reverse engineering because the graph was not written to be read. There are
dead channels, channels wired back into themselves, connections that make no structural sense, and
names that mean nothing. Proximity in the graph implies nothing about relatedness. Treat any belief
about a channel that you have not traced as a guess.

Reading is forgiving of this — a wrong read shows a wrong number and you notice. Writing is not.

## Muting is a scalpel, not a switch

[Reacting and intercepting § Choosing what to mute](hooks.md#choosing-what-to-mute-is-the-whole-problem)
covers this, but it bears repeating in the safety chapter:

A channel whose only effect is drawing can be suppressed with no consequence. A channel whose result
something else reads leaves that reader holding a stale value forever — not a crash, just the game
quietly computing wrong answers. That is the worse outcome, because nothing tells you it happened.

Find out what depends on a node before you take it away.

## What is not in the environment

These are removed after the plugin's own setup runs and before any script loads:

| Removed | Why |
|---|---|
| `ffi` | The raw memory and native-call interface. The plugin uses it to build the fast bindings, then takes it away — a script keeping it would have unrestricted access to the game's address space. |
| `io`, `os.execute`, `os.remove`, `os.rename`, `os.tmpname`, `os.exit`, `os.getenv`, `os.setlocale` | Filesystem and process access. |
| `require`, `package`, `dofile`, `loadfile` | Loading code from disk. |
| `load`, `loadstring` | Compiling code at runtime. |
| `debug` | A memory- and state-inspection surface. |
| `jit` | `jit.util` can read VM internals. |
| `newproxy`, `collectgarbage` | Sharp edges with no legitimate use here. |

Everything else — `string`, `table`, `math`, `os.clock`, `os.time`, `os.date`, coroutines — works
normally.

### This is not a security boundary

It is worth being honest about what that list buys you. It makes *honest mistakes* survivable and
keeps casual scripts from reaching outside the game. It is **not** a sandbox that will contain a
script written to be malicious, and it is not audited as one.

Running someone else's script is trusting them, exactly like running any other program they sent you.
Read scripts before you install them, the same as you would a macro or a plugin for anything else.

## What is deliberately absent

Not oversights — decisions. If one of these is blocking something you want to build, that is worth
raising; the reasoning may not survive a good use case.

**No script-to-script communication.** No shared state, no messaging, no way to detect another
script. Scripts are independent, and the only place they meet is on a shared channel, where the rules
are fixed and neither can override the other. This keeps "I installed five scripts and one broke the
others" out of the design.

**No persistence.** A script cannot write files or save settings between sessions. Everything resets
when it is reloaded.

**No input.** Scripts cannot read the keyboard or mouse and cannot draw interactive controls. What
you draw is output only.

**No access to the Tweaker's own state.** Playlists, skins, tweak toggles and the Quick Player are
not exposed yet. This is a planned addition rather than a principle.

**No creating or removing table rows.** `array:set` writes a row that exists and refuses one that
does not — see [Writing a table row](channels.md#writing-a-table-row). The engine's own row
management (add, insert, remove) sits one vtable slot away from the cell write and is deliberately
not exposed: a row of a game table is an object in the game's model, not a slot in an array.

**No images of your own.** `tw.hud.icon` draws icons **packed into the plugin**, addressed by a bare
stem that can only reach `icons/*.svg`. A script cannot load a file — not an image, not anything else
— and the icon set is what ships with the Tweaker.

That is a smaller restriction than "no images" and a deliberate one: reaching the filesystem needs a
name restriction, a per-script texture lifetime and an invalidation path for when the render device
is replaced, none of which exist yet. If you want to ship art with a script, say so.

**No arbitrary shapes.** Rectangles and lines, with glow. No circle, no polygon, no path.

**No script-defined menu tabs or settings UI.** A script's only surface is what it draws and the row
it occupies in the Scripts tab.

## Failure containment

Scripts cannot take the game down through ordinary mistakes, and the mechanisms are worth knowing so
their behaviour is not surprising.

**Errors are caught.** A handler that throws does not propagate into the game. The error is reported
in the notification strip and the log.

**One strike.** The first error switches the dispatcher off — frame handlers and channel hooks
separately. It is not retried. A handler that throws once throws sixty times a second, and the flood
is worse than the failure. Toggling any script in the Scripts tab clears the latch.

**Drawing state is snapshotted.** If a script dies halfway through drawing, the UI state it left
unbalanced is restored, so a broken script loses its own output rather than corrupting the overlay.

**Suppression fails open.** A `before` handler that errors lets the game's call through. A script can
never remove a piece of the game by crashing.

**Disabling is complete.** A disabled script's interception is removed from the game entirely, not
just bypassed. See
[Getting started § Turning a script off really turns it off](getting-started.md#turning-a-script-off-really-turns-it-off).

## Versions and stability

The API is young and this is worth setting expectations about.

**Names are stable.** Existing functions are not planned to change shape; `tw.channel` still works as
an alias years after being superseded.

**Channel indices are not stable.** Addressing a channel by index (`tw.float_ch("Group", 900)`)
depends on the exact build of Audiosurf. It is exact and it is brittle. Use names wherever names are
unambiguous, and comment every index you do use with *why* the name would not do.

**The game is not ours.** Everything here rests on reverse-engineered internals. A patch to Audiosurf
could invalidate any of it. This is unlikely — the game has been stable for a long time — but it is
the honest position.
