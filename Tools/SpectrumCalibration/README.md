# Spectrum Calibration

What frequency range is each of Audiosurf's FFT bands?

The game hands the channel graph two spectra — a 12-band `SpectrumHit` and a 256-bin
`SpectrumHit256` (`Docs/Internal/reversing-journal-gameplay.md` §10). Neither one says which
frequencies it covers, and **music cannot answer the question**: every real track has energy
everywhere at once, so "band 0 reacts to everything" is equally consistent with band 0 being
0–2 kHz and with the game weighting it oddly. Playing music and watching the meters gives an
impression, not a number.

A stepped tone gives a number. One pure sine at a time, at a frequency we chose, with nothing else
playing: whatever lights up **is** that frequency's band.

## Doing the measurement

```
uv run --with numpy --with soundfile python make_calibration.py
```

Writes `Audiosurf Spectrum Calibration.wav` (~61 s, 5 MB) — 61 tones from 20 Hz to 20 480 Hz, one
per sixth of an octave, each held 0.75 s at −6 dBFS with silence between them.

Then, in the game:

1. Enable **Spectrum Calibration** in the Scripts tab (`assets/scripts/spectrum_calibration.lua`).
2. Play the file. Any mode; the ride does not matter.
3. Watch until the end. The script prints each band's passband in Hz and the Hz-per-bin of the wide
   spectrum.
4. Turn the script off afterwards — it reads 256 array cells per frame while sampling.

The script refuses to record against anything whose length is not the calibration track's, so an
ordinary song played with it enabled cannot fill the table with nonsense that looks like data.

## Checking the tools before trusting the answer

Both halves check themselves, and both checks have already caught something real.

**The generator** reads its own output back and FFTs every step. It caught two defects that would
otherwise have been blamed on the game: handing libsndfile the whole minute in one call overflows
the stack inside its Vorbis writer (leaving a plausible truncated file behind), and at libsndfile's
default quality Vorbis lowpasses hard enough that the 20 480 Hz step comes back as **silence** — a
missing top octave that would have read as "the game has no band up there". Hence WAV: `.WAV` goes
down the same BASS path as `.OGG` inside the game, so the encoder was buying nothing.

**The reader** is exercised against a simulated game:

```
uv run --with lupa python run_tests.py
```

`test_reader.lua` loads the real `spectrum_calibration.lua` off disk, stubs `tw`, and drives it
through a synthetic playthrough — twice: once with linearly spaced bands and once with logarithmic
ones. It then reads the conclusions back out of what the script drew. **A reader worth trusting has
to report two different answers**; if both runs agreed it would be echoing its own assumptions
rather than measuring. Both mutations tried (defeating the half-power threshold, corrupting the
least-squares fit) turn the relevant checks red.

LuaJIT 2.1 via `lupa`, because that is the VM the plugin embeds. `minilua` — the one Lua binary the
LuaJIT build already produces — is stripped to what DynASM needs and has neither `math` nor
`tostring`, so it cannot load the script under test at all.

## The schedule lives in two places

`make_calibration.py` and `spectrum_calibration.lua` each carry the step schedule, and nothing
reconciles them: the reader is a sandboxed script with no file access, so the numbers cannot be
handed over. The generator prints its copy at the end of a run; the reader prints the song length it
expects and refuses to record when the actual one disagrees. A mismatch is therefore visible rather
than silent, which is the most that can be done short of merging the two.

## Files

| File | What |
|---|---|
| `make_calibration.py` | writes the stepped-tone WAV and verifies every step |
| `test_reader.lua` | drives the real reader against a simulated game |
| `run_tests.py` | launcher: LuaJIT 2.1 via `lupa` |
| `Audiosurf Spectrum Calibration.wav` | generated, gitignored — 8 KB of script reproduces it byte for byte |
