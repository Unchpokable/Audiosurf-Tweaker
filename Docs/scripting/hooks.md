# Reacting and intercepting

[← back to the index](../scripting.md)

Reading channels every frame tells you what the game's state *is*. Hooks tell you when something
*happened* — and, if you want, stop it happening at all.

## Reacting: `tw.on_call`

```lua
tw.on_call("TrafficCommander", "Do_CollectCar", function()
    tw.notify("collected a block")
end)
```

Your function runs on the game's own call stack, the instant that channel is called. This is not
polling: nothing is missed, and there is no frame of latency.

Register at the top level of your script. Like channel handles, registration is retried until the
group is loaded, so it is fine to ask for something that does not exist yet.

### `after` and `before`

```lua
tw.on_call(group, name, "after", fn)    -- default
tw.on_call(group, name, "before", fn)
tw.on_call(group, name, fn)             -- same as "after"
```

**`after`** runs once the game's handler has returned. This is what you want almost always: the
handler has done its work, so whatever it wrote is readable.

```lua
local colour = tw.float_ch("TrafficCommander", 900)

tw.on_call("TrafficCommander", "Do_CollectCar", "after", function()
    local c = colour:get()      -- written at the top of the handler; valid now
    -- ...
end)
```

**`before`** runs first, while the state is still whatever the previous frame left. Useful when you
need the *old* value, and required if you want to cancel.

### Your callback gets no arguments

The game does not pass parameters the way a function call does — a handler's inputs are other
channels it reads. So the way to know *what* happened is to read the channels the handler works
with, inside your callback.

Which is why `after` is the default: by then, they hold this event's values.

## Intercepting: returning `false`

A **`before`** handler that returns `false` stops the game's own handler from running.

```lua
tw.on_call("SomeGroup", "Do_Something", "before", function()
    return false        -- the game's handler never runs
end)
```

Any other return value — including returning nothing — lets it through. That is deliberate: an
observer can never suppress something by accident, and a callback that hits an error still lets the
game proceed.

Returning `false` from an `after` handler does nothing. The call it would decline already happened.

**A suppressed call has no `after` phase.** Not for you, and not for any other script watching the
same channel. `after` means "the original returned", and it did not.

## Switching game behaviour off: `tw.mute`

If all you want is for a channel to stop doing its job, `tw.mute` is the direct way:

```lua
local medals = tw.mute("XX_gui", "Do_LadderMedalRequirements")
```

That is the game's in-run medal display, gone. The channel is still in the graph and the game still
calls it every frame; it simply does nothing.

The returned handle can be flipped without re-hooking:

```lua
medals:off()        -- let the game do it again
medals:on()         -- suppress it again
medals:set(enabled) -- either
medals:active()     -- is it suppressing right now?
```

It starts on.

### Why a separate function

`tw.mute` is `tw.on_call(..., "before", function() return false end)` with the round trip into Lua
removed. The things worth muting are usually render nodes the game calls sixty times a second
forever, and crossing into the VM that often to answer "no" is pure cost. A mute is a predicted
branch in native code.

### Choosing what to mute is the whole problem

The API will happily suppress any channel. Whether that is *safe* is a question about the game's
graph, and nothing here can answer it for you.

The distinction that matters:

- **A channel whose only effect is drawing** can be removed with no consequence. The medal dashboard
  is a pure render branch — it draws rings and a cover shape and nothing downstream reads anything it
  produces. Muting it is clean.
- **A channel whose result something else reads** leaves that reader holding a stale value forever.
  That is not a crash; it is the game quietly computing wrong answers, which is worse.

Before muting something, find out what else depends on it. The reverse-engineering notes in
`Docs/Internal/` are where that information lives.

Prefer to mute the **most specific** node that achieves what you want. Muting a handler high in the
tree takes out everything below it, including things you did not mean.

## When several scripts want the same channel

This is expected, not exceptional — install a few scripts and two of them will eventually watch the
same popular handler. The rules:

- **Every `before` handler runs**, in subscription order, *before* any of them can cancel. So whether
  your observer sees an event never depends on which script the user happened to install first.
- **Cancellation is an OR.** Any one of them can suppress the call. Two scripts muting the same node
  compose, and turning one off leaves it muted by the other.
- **A suppressed call has no `after` phase for anyone.**
- Then every `after` handler runs.

You cannot detect or override another script. That is intentional — there is no ordering to fight
over and no priority to escalate.

The Scripts tab shows a count of channels more than one script is attached to, so a user can at least
see when it is happening.

## Events worth knowing about

| | |
|---|---|
| `StatCollector` · `Do_ResetStats` | A new run is starting. The reliable place to zero your counters — better than watching the song timer jump backwards, which is a guess about this. |
| `StatCollector` · `Do_CalculateFinalStats` | The run ended and scoring is happening. After this, the final score and `Feat String` are readable. |
| `TrafficCommander` · `Do_CollectCar` | Your car took a block off the road — through the board, Pointman's buffer, or an Eraser shatter. |
| `Puzzle` · `Do_ResovleLaneCrash` | A block landed in the grid. (The misspelling is the game's.) |

## Events versus state

A genuine design question, and getting it wrong is a classic mistake, so it is worth stating plainly.

**Use state for "what is true right now". Use hooks for "this just happened".**

Say you want your HUD hidden while the player is in a menu. It is tempting to hook the transitions —
song started, went to menu, paused, unpaused — and keep a flag. Do not. A flag assembled from events
is only correct if it saw *every* event: enable your script mid-run and it starts out wrong, miss one
path back to the menu and it stays wrong until the next one.

The game already knows:

```lua
local state    = tw.float_ch("StartGroup", "StartupState")
local gameplay = tw.float_ch("StartGroup", "State_Gameplay")

local function playing()
    local s, g = state:get(), gameplay:get()
    if s == nil or g == nil then return true end   -- can't tell: don't hide a working widget
    return math.abs(s - g) < 0.5
end
```

Two reads a frame, correct on the first frame, and nothing to desync.

Hooks remain the right tool for moments — `Do_ResetStats` is an *event*, and there is no state to
read that means "a run just began".

## Cleanup

You do not unregister anything. When a script is disabled, everything it registered is removed and
the game's own dispatch is restored — see
[Getting started § Turning a script off really turns it off](getting-started.md#turning-a-script-off-really-turns-it-off).

## Next

[Drawing](drawing.md) — putting something on screen.
