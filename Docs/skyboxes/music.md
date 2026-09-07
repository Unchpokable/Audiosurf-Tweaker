# Reacting to the music

[← back to the index](../skyboxes.md)

Audiosurf builds its track from the song, and it already computes a spectrum every frame to do it.
Your shader can read that. Nothing is intercepted and no audio is touched — the game publishes these
numbers and the plugin passes them along.

Four registers, `c208`–`c211`, all declared in `sky_common.hlsli`:

```hlsl
float4 g_music      : register(c208); // x = raw total, y = body, z = onset, w = valid
float4 g_music_time : register(c209); // x = seconds, y = length, z = 0..1, w = since onset
float4 g_music_eq   : register(c210); // x = 0..1.8k, y = 1.8..3.6k, z = 3.6..5.4k, w = 5.4k+
float4 g_music_slow : register(c211); // x = 0.5 s, y = 1.5 s, z = 4 s, w = 10 s
```

Declare only what you read. A sky that mentions none of them costs nothing at all — the plugin does
not even read the game's channels for it.

---

## The five-minute version

```hlsl
// A glow that swells with the music and dies away in silence.
const float drive = g_music_slow.y * g_music.w;
scene += glow_colour * (glow * drive * g_sensitivity);
```

Three things in that line matter, and each of them is a mistake someone has already made.

**`g_music.w`** is 1 only when the numbers are real — not in a menu, not between songs, not before
the game's graph is reachable. Multiply by it and your effect settles gracefully; ignore it and your
sky freezes on whatever it last saw.

**`g_music_slow.y`, not `g_music.x`.** See the next two sections.

**`g_sensitivity`** is a knob of yours, not a constant. How loud a song gets is not something you can
predict from here.

---

## Everything is already smoothed

A pixel shader has no state. It cannot average, cannot remember the previous frame, cannot low-pass
anything. So a raw per-frame number handed to it is noise with no way to remove it, and **every
value in these registers is filtered before it arrives.**

The one exception is `g_music.x`, which is deliberately raw and which you should not be using
anyway — see below.

---

## Which number to drive things from

### `g_music.y` — the body

Everything below 3.6 kHz, smoothed with a fast attack and a slow release. That range is where the
fundamentals live: bass, voice, strings, guitars. A sustained note holds it up for as long as it
sounds.

**This is the default choice** for anything that should track the music closely — an effect that
should land on a beat.

### `g_music_slow` — the same body over longer windows

Four rungs: **0.5 s, 1.5 s, 4 s, 10 s**. One-pole time constants, so read them as "the last second
or so" rather than as a fixed window. Symmetric — they rise and fall at the same rate.

This is what `g_music.y` cannot do. On dense, many-voiced music the body changes every note, and an
effect driven by it reads as *flicker* even though it is following the audio exactly. What the ear
tracks through a busy passage is the shape of the phrase, not the individual notes.

Pick a rung by what the effect is **for**:

| | |
|---|---|
| should hit on the beat | `g_music.y` |
| should breathe with the bar | `g_music_slow.x` (0.5 s) |
| should swell through a phrase | `g_music_slow.y` (1.5 s) |
| should follow verse vs. chorus | `g_music_slow.z` or `.w` (4 s, 10 s) |

Interpolating between two rungs is fine and is how to land between them. Offering the choice as a
knob is better still — the bundled sky's arc has a `Music window` slider that walks the ladder, so
the player tunes the timescale instead of you guessing it.

### `g_music.z` — the onset pulse

1.0 the frame a hit is detected, decaying afterwards. Detected on the body, so it fires on a kick
drum rather than on a hi-hat.

Use it for flashes. **Adding it on top of a slow envelope brings the flicker straight back**, which
is why the bundled sky ships with its punch knob at zero and leaves it to the player.

### `g_music.x` — the game's own total, and why not to use it

It is the sum of all twelve of the game's spectrum bands. Those bands are **linear** — 1808.7 Hz
each, measured with a stepped tone — so nine of the twelve sit above 5.4 kHz, where music has
nothing but cymbals, breath and distortion.

Driven directly in game, it twitched on sounds that were not audible and sat still under a sustained
bass line. It is exposed because it is the game's own number and 179 of the game's own consumers use
it, not because it is the one you want.

---

## The spectrum, and what it cannot tell you

`g_music_eq` is the twelve bands folded into the only four groups they can honestly support:

| | | |
|---|---|---|
| `x` | 0 – 1.8 kHz | bass, voice, strings, guitars — every fundamental in the music |
| `y` | 1.8 – 3.6 kHz | upper harmonics, presence |
| `z` | 3.6 – 5.4 kHz | brilliance |
| `w` | 5.4 kHz and up | cymbals, breath, hiss, distortion |

Nine of the game's bands share that last slot because nine of them share one meaning.

**There is no kick drum here.** A bin is 86 Hz wide and band 0 spans 0–1.8 kHz, so a bass drum, a
bass line and a vocal all land in `x` together, and no processing on our side can separate them —
the difference is not in the data. If you want an effect that pumps specifically on the kick, this
API cannot do it yet. That needs the plugin to compute its own, finer FFT, which is planned and will
arrive behind these same names.

That is the reason these are named `low`/`mid`/`high`/`air` and not `band[k]`: when the source
improves, your sky does not change.

---

## Song position

`g_music_time` is `x` = seconds into the song, `y` = its length, `z` = progress 0–1, `w` = seconds
since the last onset.

`z` is the useful one — a sky that shifts palette across a track, or a horizon that rises as the run
goes on. `w` lets you write your own decay curve when the built-in onset pulse is the wrong shape.

---

## Making it tunable rather than tuned

How loud a song gets is not knowable from inside the shader, so a hard-coded multiplier will be
wrong for someone. The shape that works:

```hlsl
float4 g_reactive : register(c46);  // x = amount, y = gain, z = punch, w = floor

const float driven = saturate(g_music_slow.y * g_reactive.y
                            + g_music.z * g_reactive.z) * g_music.w;

const float music = max(saturate(g_reactive.w), driven);

// amount 0 -> the effect exactly as you authored it, static.
// amount 1 -> entirely the music's.
const float response = saturate(lerp(1.0, music, saturate(g_reactive.x)));
```

- **amount** makes turning the feature off a slider rather than an edit.
- **gain** decides whether the effect blooms only at peaks or lives near full most of the time.
- **floor** is what survives silence. Zero is genuinely nothing.

Then use `response` to scale **more than one property**. An effect that only gets dimmer reads as the
same object behind a filter; one that also shortens, narrows or shrinks reads as *growing*. The
bundled sky's arc scales its span, its width and its brightness from one number, and that is what
makes it feel alive rather than dimmed.

One caution from doing exactly that: give width a floor (`lerp(0.35, 1.0, response)`) while letting
span and brightness reach zero. A width that reaches zero hits the pixel floor and hands back a
ragged one-pixel line at precisely the moment the effect is supposed to be disappearing.

---

## Finding the right gain

Do not guess it. The Tweaker ships a Lua diagnostic, `music.lua`, that draws the live values and
their range over the session, and prints the gain that would reach full response at the song's peak:

```
body 0.412  (low 0.318  mid 0.094  high 0.021  air 0.058)   vs the game's total 0.630
body over this session: 0.000 .. 0.712  -> a Music gain of about 1.40 reaches full arc at the peak
```

Enable it in the Scripts tab, play a track you care about, read the number off the screen.

Note that the number applies to whichever value you are driving from. A slow rung has lower peaks
than the fast body by definition, so changing the window means re-touching the gain.
