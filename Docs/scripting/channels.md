# Reading and writing the game

[← back to the index](../scripting.md)

## Accessors, one per kind

There is no generic "read a channel". You say what kind of channel you expect:

```lua
local points = tw.float_ch("StatCollector", "Points")        -- a number
local feats  = tw.string_ch("StatCollector", "Feat String")  -- text
local colour = tw.vector_ch("StartGroup", 1542)              -- three numbers
```

This is deliberate, and it is a safety property rather than a style choice. Inside the engine, the
same slot in a channel's dispatch table means "read a number" on one kind of channel and "read a
string" on another — and on a kind that has neither, it is not a function at all. Guessing would not
give you a wrong number; it would jump into whatever happened to be next in memory.

So the accessor *is* the type declaration. Ask through the wrong one and you get an immediate,
specific error:

```
StatCollector.Feat String is a 'text' channel, not 'float' - use the matching accessor
```

That is a bug in the script, it can never fix itself, and so it is loud.

> `tw.channel` still exists as an alias for `tw.float_ch`, for scripts written before the typed
> accessors existed. New code should say which kind it means.

## Creating a handle is not reading

```lua
local points = tw.float_ch("StatCollector", "Points")   -- cheap, does nothing yet
local value  = points:get()                             -- this reads
```

Creating a handle costs nothing and never fails. It is a *promise* to find that channel later.

**Create handles once, at the top level of your script.** Do not create them inside `on_frame`.
Finding a channel by name is a linear scan over every channel in the group — thousands of string
comparisons — and a handle caches the result after the first success. A handle created per frame
never gets to cache anything.

```lua
-- good
local points = tw.float_ch("StatCollector", "Points")
tw.on_frame(function()
    local v = points:get()
end)

-- bad: rescans the whole group every frame, forever
tw.on_frame(function()
    local v = tw.float_ch("StatCollector", "Points"):get()
end)
```

## Nothing is available immediately

**Every `:get()` can return `nil`, and will, for a while after the game starts.**

Three reasons, all normal:

- the Tweaker has not got hold of the game's engine yet — it captures that the first time the game
  runs a certain kind of channel, which in practice can mean "after the player clicks something";
- the group is not loaded (you are in the menu and asked for something that only exists in a run);
- the channel genuinely does not exist (a typo).

The handle keeps retrying by itself, roughly once a second, and caches the answer once it succeeds.
You do not manage any of that. What you *do* have to do is handle `nil`:

```lua
tw.on_frame(function()
    local score = points:get()
    if score == nil then return end        -- nothing to show yet

    -- ... use score
end)
```

`nil` rather than `0` is a deliberate choice: a script showing `--` until the game is reachable is
telling the truth, one showing `0.0` is lying.

If you want to check without reading, `handle:valid()` says whether it has resolved yet.

For a whole-script gate, `tw.engine_ready()` answers "is the graph reachable at all":

```lua
tw.on_frame(function()
    if not tw.engine_ready() then return end
    -- ...
end)
```

### The one thing that is not silent

Transient failures stay quiet, because they are normal. But a typo looks exactly like a transient
failure at first, and a typo that stays silent forever is worse than a noisy one.

So after several failed attempts, a handle reports itself **once**:

```
Lua: StatCollector.Pointz: no such channel in group
```

If you see that, it is a mistake, not timing.

## Numbers

```lua
local points = tw.float_ch("StatCollector", "Points")

local v = points:get()      -- number, or nil
points:set(1000)            -- returns true if it was written
```

Everything numeric in the game is a float, including flags (`0` / `1`) and enumerations.

### Writing

`:set()` works, and on an ordinary value channel it is a plain store with no side effects. But this
is the sharpest edge in the whole API, and it is worth being blunt about why:

**The API can guarantee the write is well-formed. It cannot guarantee it is sensible.** The engine
will not crash from being handed a number. The *game* might do something terrible with it — index a
table out of range, divide by it, or simply desync its own bookkeeping.

Rules of thumb:

- Writing to a value the game only reads (a threshold, a colour) is usually fine.
- Writing to a value the game also writes is a race you will lose — it will overwrite you next frame.
- Writing to a table cursor breaks whatever the game reads next. Use `tw.array`, which restores it.
- Anything with a plausible valid range probably has one.

Read [Limits, safety and performance](limits.md) before you start writing to things.

## Text

```lua
local feats = tw.string_ch("StatCollector", "Feat String")

local s = feats:get()      -- a Lua string, or nil
```

The engine stores some strings as wide characters (song titles, anything localised); the conversion
is handled for you and you always get a normal Lua string.

An unreadable text channel gives `""`, not `nil` — but a *missing* one still gives `nil`.

## Vectors

Vector channels hold three numbers. The game keeps its live block palette in them, which is what
makes this useful: you can colour your own UI with the player's actual colour settings instead of
guessing.

```lua
local purple = tw.vector_ch("StartGroup", 1542)

local r, g, b = purple:get()          -- three numbers, or nil
if r then
    local packed = tw.rgb(r, g, b)    -- ready to draw with
end
```

Three return values rather than a table, on purpose: a table per read would be garbage collected
every frame on the drawing path, and every caller wants the components immediately anyway.

## Tables

A table column plus its cursor, as one object:

```lua
local counts = tw.array("StatCollector", "Stats: TrafficColorCounts", "Index_TrafficColorCounts")

local reds = counts:get(4)       -- number, or nil
```

`tw.array_vec` is the same for columns of vectors, and gives three numbers:

```lua
local pattern = tw.array_vec("StatCollector", 57, "Index_TrafficPattern")

local colour_id = pattern:get(0)   -- x of row 0
```

Both save the cursor, move it, read, and put it back — see
[How Audiosurf is built § Tables and cursors](game-model.md#tables-and-cursors) for why that last
part is not optional.

### Walking a big table

Some tables are large. The generated track can be several thousand rows, and each row costs a cursor
write, a read and a restore. Doing all of that in one frame is a visible stutter at exactly the
moment the player is starting a run.

Spread it out:

```lua
local ROWS_PER_FRAME = 128
local at, total, done = 0, 0, false

tw.on_frame(function()
    if done then return end

    local budget = ROWS_PER_FRAME
    while at < total and budget > 0 do
        local value = pattern:get(at)
        if value == nil then return end        -- not readable yet; resume next frame

        -- ... accumulate

        at = at + 1
        budget = budget - 1
    end

    if at >= total then done = true end
end)
```

## Addressing by index

When a name is ambiguous, pass a number instead of a string. It works everywhere a name does:

```lua
tw.float_ch("TrafficCommander", 900)
tw.array_vec("StatCollector", 57, "Index_TrafficPattern")
tw.on_call("TrafficCommander", 763, function() end)
```

An index is a position within that group in a specific build of the game. It is exact, and it is the
thing most likely to break if the game is ever patched. Use a name when the name is unambiguous.

## Diagnostics

```lua
tw.engine_ready()        -- is the graph reachable at all?
handle:valid()           -- has this particular handle resolved?
tw.groups()              -- table of "<pool name> | <file>" for every loaded group
```

`tw.groups()` is the answer to "why will my group not resolve". Print it when you are stuck — the
list changes as the game moves between menus and runs, and seeing what is actually there usually
ends the investigation immediately.

## Next

[Reacting and intercepting](hooks.md) — running code when the game does something.
