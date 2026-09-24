# Worked examples

[← back to the index](../scripting.md)

Three scripts that were written to answer a question about the game, not to do anything for a player.
They are here rather than in your `Scripts\` folder for exactly that reason — nobody wants a probe
running while they play — but they are the most honest documentation of the API there is: each one
was used to establish something the rest of these pages now states as fact.

Copy any of them into `engine\TweakerStuff\Scripts\`, open the overlay's Scripts tab and switch it
on. See [Getting started](getting-started.md) if that layout is new to you.

The full versions, with the whole investigation written out in comments, live in the repository under
`TweakerPlugin/assets/scripts/dev/`.

---

## Writing a vector channel, and proving the write stuck

**Demonstrates:** `tw.vector_ch`, `:get`/`:set` on a vector, HUD drawing, and the general shape of
measuring something that is only true some of the time.

> The scripts below still open their handler with `if not tw.engine_ready() then return end`. That
> line is now dead: a handler does not run at all until the graph is up. It is left in because these
> are transcripts of scripts that exist on disk and were used as written — and because a redundant
> guard is worth seeing once, so you recognise it in older scripts you may be handed and know it can
> go.

Writing a vector channel is not the local store that writing a number is. The engine sets the three
components and then writes each one through into whatever numeric channel is wired to it — so
whether your value survives depends on what is on the other end. Plain values keep it. Computed
channels re-derive and throw it away, typically on the very next frame. See
[Reading and writing the game § Vectors](channels.md#vectors).

The instructive part is not the write, it is the measurement. The first version of this script
printed a verdict from the current frame — and a contested channel alternates, so the line changed
every frame, and at 100+ Hz two different strings at the same position blur into each other. The
instrument destroyed its own reading. Counting over a window says strictly more: not "does it hold"
but "how often".

```lua
-- @name        Vector Write Probe
-- @description Writes a vector channel and reads it back, to show whether the write stuck

local GROUP, TARGET = "XX_gui", "Color1"

-- Off by default: this writes a colour the player really sees. Flip it and press Reload.
local WRITE = false
local VALUE = { 0.1, 1.0, 0.2 }     -- components are 0..1

local target = tw.vector_ch(GROUP, TARGET)

local last_write = { 0, 0, 0 }
local WINDOW = 120
local samples, held = 0, 0

tw.on_frame(function()
    if not tw.engine_ready() then return end

    -- Read before writing: this is what survived from last frame's write, which is the question.
    local r, g, b = target:get()
    if r == nil then return end

    if WRITE then
        target:set(VALUE[1], VALUE[2], VALUE[3])
        last_write = VALUE
    end

    local drift = math.abs(r - last_write[1]) + math.abs(g - last_write[2]) + math.abs(b - last_write[3])
    samples = samples + 1
    if drift < 0.001 then held = held + 1 end

    -- Restart the window rather than sliding it: a sliding average hides a regime change, and going
    -- in and out of a run is exactly when the answer is expected to change.
    if samples >= WINDOW then samples, held = 0, 0 end

    local ratio = samples > 0 and held / samples or 0
    local verdict
    if samples < 10 then verdict = "sampling"
    elseif ratio > 0.99 then verdict = "holds"
    elseif ratio < 0.01 then verdict = "always overwritten"
    else verdict = "contested - re-derived on some frames" end

    local x, y = tw.hud.safe()
    tw.hud.text(x + 24, y + 24, string.format("%s::%s  held %d/%d  %s", GROUP, TARGET, held, samples, verdict))
end)
```

Two results worth carrying away, both of which cost a rewrite to learn:

- **A channel reading sensibly is not evidence the game reads it.** An earlier version wrote to a
  palette the game only consults as a fallback. The write landed, a script reading it back saw the
  new colour, and the blocks on the road stayed exactly as they were.
- **The write persists after the script stops.** It goes into plain values, so switching the script
  off does not put the old colour back. The full version captures the original on the first readable
  frame and has a `RESTORE` flag to put it back.

---

## Writing a table row, and checking the guard

**Demonstrates:** `tw.array`, `tw.can_write`, row bounds, `tw.hud.measure`.

Two claims about table writes can only be confirmed inside the running game, and one of them is a
safety guard whose failure would be silent: an out-of-range row index must be **refused**, not
written somewhere else. A guard nobody has watched fail is a guard nobody knows works.

```lua
-- @name        Array Write Probe
-- @description Checks that writing a table row works, and that an out-of-range row is refused

local WRITE = false     -- set to true and Reload to actually write

local counts = tw.array("StatCollector", "Stats: TrafficColorCounts", "Index_TrafficColorCounts")
local SENTINEL = 1234.5

-- Latched, not recomputed per frame: a readout that changes every frame is unreadable at 180 Hz and
-- destroys its own measurement. This project has made that mistake once already.
local done, lines = false, {}
local function record(text) lines[#lines + 1] = text end

local function run()
    local rows = counts:rows()
    if not rows or rows < 1 then return false end       -- not reachable, or still empty
    if not WRITE then record("not writing - set WRITE = true and Reload"); return true end
    if not tw.can_write() then return false end          -- write gate still shut; try again next frame

    -- Claim 1: an in-range write lands and reads back.
    local original = counts:get(0)
    if not original then return true end

    local wrote = counts:set(0, SENTINEL)
    local readback = counts:get(0)
    counts:set(0, original)                              -- put it back before anything else looks

    record(wrote and "in-range write accepted" or "in-range write REFUSED - unexpected")
    record(string.format("read back %.1f (wanted %.1f)", readback or -1, SENTINEL))

    -- Claim 2: out of range is refused - and, the part that actually matters, the table does not
    -- grow. A refusal that still lengthened the table would be worse than no check at all, because
    -- it would look like it had worked.
    local past = counts:set(rows, SENTINEL)
    local negative = counts:set(-1, SENTINEL)

    record(not past and "row past the end refused" or "row past the end was ACCEPTED - guard broken")
    record(not negative and "negative row refused" or "negative row was ACCEPTED")
    record(counts:rows() == rows and "table length unchanged" or "TABLE GREW")
    return true
end

tw.on_frame(function()
    if not done then done = run() end

    local x, y = tw.hud.safe()
    for i, line in ipairs(lines) do
        tw.hud.text(x + 24, y + 24 + (i - 1) * (tw.hud.font_size() + 5), line)
    end
end)
```

Note `tw.can_write()` — it reports whether writes are accepted at all, which they are not while the
game is still loading. In a probe it is a reason to wait; in a script that does something real it is
the check to make before touching anything.

`array:set` is the one write in this API that refuses an out-of-range index instead of attempting
it, and this script exists to keep that honest. The engine's own write path *creates* the missing
row, which would lengthen a table the rest of the game reads.

---

## Measuring what the game's spectrum bands actually cover

**Demonstrates:** `tw.float_ch` against the audio channels, `tw.array`, accumulating over a whole
track, `tw.hud.rect` for drawing a graph.

This one exists because music cannot answer the question it asks. Every real track has energy
everywhere at once, so "band 0 reacts to the kick drum" is a guess dressed as a measurement. Feed
the game a track that is a sequence of pure tones with known frequencies instead, watch which band
lights up for each, and the mapping falls out.

It is the reason the skybox music interface can say what each band means — see
[Skyboxes § Music](../skyboxes/music.md) and
[the engine's audio channels](game-model.md). It needs the calibration track that goes with it
(`Tools/SpectrumCalibration/` in the repository), which is why it is a poor fit for a normal
install: without that track it measures nothing.

The shape of it, with the bookkeeping and the drawing trimmed:

```lua
-- @name        Spectrum Calibration
-- @description Measures which frequencies the game's FFT bands actually cover

local GROUP = "VisMusic"

-- The schedule of the calibration track. make_calibration.py prints these and they must match:
-- this is a sandboxed script with no file access, so there is no way to hand them over.
local CAL_LEAD_IN, CAL_STEP, CAL_SILENCE = 3.0, 0.9, 0.15
local CAL_FIRST_HZ, CAL_PER_OCTAVE, CAL_STEPS = 20.0, 6, 61

-- Sample the settled middle of each tone, well inside the raised-cosine edges.
local SAMPLE_FROM, SAMPLE_TO = CAL_SILENCE + 0.30, CAL_STEP - 0.05

local ch = {
    time   = tw.float_ch(GROUP, "RunningTime_Seconds"),
    length = tw.float_ch(GROUP, "SongLength"),
}

-- The spectrum is a table, not a channel per band: one column plus the cursor that indexes it.
local spectrum = tw.array(GROUP, "New Table: SpectrumHit", "SpectrumIndex")

local rows = {}     -- rows[step] = { bands = {…}, frames = n }

local function step_hz(index)
    return CAL_FIRST_HZ * 2.0 ^ (index / CAL_PER_OCTAVE)
end

tw.on_frame(function()
    if not tw.engine_ready() then return end

    local t = ch.time:get()
    if t == nil or t < CAL_LEAD_IN then return end

    -- Which tone is playing, and are we in its settled middle?
    local into = t - CAL_LEAD_IN
    local step = math.floor(into / CAL_STEP)
    local phase = into - step * CAL_STEP
    if step < 0 or step >= CAL_STEPS then return end
    if phase < SAMPLE_FROM or phase > SAMPLE_TO then return end

    local row = rows[step] or { bands = {}, frames = 0 }
    for band = 0, 11 do
        local v = spectrum:get(band)
        if v then row.bands[band] = math.max(row.bands[band] or 0, v) end
    end
    row.frames = row.frames + 1
    rows[step] = row

    -- …and then draw rows as a heatmap: one column per tone, one cell per band. A diagonal means
    -- the schedule above matches the track; a smear means it does not.
end)
```

Two details in there are worth more than the measurement:

- **The spectrum is a table channel, not one channel per band.** `tw.array` with a column and its
  cursor is how you read it; the cursor is saved and restored around each access for you.
- **The script refuses to record against anything but the calibration track**, by checking
  `SongLength` against the schedule. Without that, one ordinary song played with the script left on
  fills the table with nonsense that looks exactly like data.

---

## Next

[Reading and writing the game](channels.md) — the rules these three were written to establish.
