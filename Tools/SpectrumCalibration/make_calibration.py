"""Generate the calibration track that tells us what Audiosurf's FFT bands actually are.

    uv run python make_calibration.py [out_dir]

The game hands the channel graph two spectra (reversing-journal-gameplay.md §10): a
12-band `SpectrumHit` and a 256-bin `SpectrumHit256`. Neither one documents which
frequencies it covers, and music cannot answer that - every real track has energy
everywhere at once, so "band 0 reacts to everything" is equally consistent with band 0
being 0-2 kHz and with the game weighting it oddly.

A stepped tone can answer it. One pure sine at a time, at a frequency we chose, held
long enough to read: whatever lights up *is* that frequency's band, with nothing else
playing to argue about it.

Steps rather than a continuous sweep, and this is the part that matters at 60 fps: a
sweep smears across bins for as long as the FFT window is, and the reader has to guess
where it was when it sampled. A step is flat for 0.75 s, so every frame inside it sees
the same signal and disagreement between frames means a real effect rather than timing.

Each step gets its own silence, so a missed step shows up as a hole rather than as an
off-by-one that shifts everything after it. Edges are raised-cosine: a hard start is a
click, a click is broadband, and broadband in a single-frequency measurement is exactly
the thing being measured.

The schedule below is repeated on the reader's side (assets/scripts/music.lua). It is
three numbers, they are printed at the end of this script, and the reader prints its own
copy on screen - so a mismatch is visible rather than silent. There is no way to hand the
numbers over automatically: the reader is a sandboxed Lua script with no file access.
"""
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

SAMPLE_RATE = 44100

# ---- the schedule. music.lua must agree with these three. -------------------------
LEAD_IN_SECONDS = 3.0     # silence before the first step
STEP_SECONDS = 0.9        # one step: silence then tone
FIRST_HZ = 20.0           # centre of step 0
STEPS_PER_OCTAVE = 6
STEP_COUNT = 61           # 20 Hz .. 20480 Hz, ten octaves
# -----------------------------------------------------------------------------------

SILENCE_SECONDS = 0.15    # the gap at the head of every step
EDGE_SECONDS = 0.010      # raised-cosine fade at each end of the tone
TAIL_SECONDS = 3.0
AMPLITUDE = 0.5           # -6 dBFS, the same for every step: response differences are
                          # then the game's weighting rather than ours


def step_hz(index):
    return FIRST_HZ * 2.0 ** (index / STEPS_PER_OCTAVE)


def build():
    total = LEAD_IN_SECONDS + STEP_COUNT * STEP_SECONDS + TAIL_SECONDS
    out = np.zeros(int(round(total * SAMPLE_RATE)), dtype=np.float64)

    tone_seconds = STEP_SECONDS - SILENCE_SECONDS
    tone_len = int(round(tone_seconds * SAMPLE_RATE))
    edge = int(round(EDGE_SECONDS * SAMPLE_RATE))

    # One window, reused: the fade is a property of the step shape, not of the pitch.
    window = np.ones(tone_len)
    ramp = 0.5 - 0.5 * np.cos(np.linspace(0.0, np.pi, edge))
    window[:edge] = ramp
    window[-edge:] = ramp[::-1]

    t = np.arange(tone_len) / SAMPLE_RATE

    for i in range(STEP_COUNT):
        hz = step_hz(i)
        if hz >= SAMPLE_RATE / 2:
            raise SystemExit(f"step {i} at {hz:.0f} Hz is past Nyquist")

        start = int(round((LEAD_IN_SECONDS + i * STEP_SECONDS + SILENCE_SECONDS) * SAMPLE_RATE))
        # Phase starts at zero every step; the fade-in makes that inaudible and keeps
        # every step byte-identical to a standalone rendering of it.
        out[start:start + tone_len] = AMPLITUDE * np.sin(2.0 * np.pi * hz * t) * window

    return out


def verify(path):
    """Read the encoded file back and check every step is the tone it should be.

    Not ceremony. The encoder is the one part of this that is not ours, it already
    failed once (a whole-array write overflows libsndfile's Vorbis stack), and a file
    that encodes to something subtly wrong would send the reader hunting for a defect
    in the game. Measuring the artifact is cheaper than trusting it.
    """
    audio, rate = sf.read(path, dtype="float64", always_2d=False)
    assert rate == SAMPLE_RATE, f"sample rate came back as {rate}"

    tone_seconds = STEP_SECONDS - SILENCE_SECONDS
    worst = 0.0

    for i in range(STEP_COUNT):
        hz = step_hz(i)
        # The steady middle of the step, clear of both fades.
        start = (LEAD_IN_SECONDS + i * STEP_SECONDS + SILENCE_SECONDS + EDGE_SECONDS * 2)
        length = tone_seconds - EDGE_SECONDS * 4
        a = int(round(start * SAMPLE_RATE))
        b = a + int(round(length * SAMPLE_RATE))

        chunk = audio[a:b] * np.hanning(b - a)
        spectrum = np.abs(np.fft.rfft(chunk))
        k = int(np.argmax(spectrum))

        # Quadratic interpolation across the peak and its neighbours. Without it the
        # answer is quantised to the FFT's own bin width - about 1.5 Hz here - and the
        # 31.7 Hz step reads as 32.4 Hz purely because that is where the nearest bin is.
        # That is the measurement's resolution showing up as if it were an encoding
        # error, and loosening the tolerance to absorb it would have thrown away the
        # ability to catch a step landing on its neighbour, which at the low end is only
        # 3.9 Hz away.
        if 0 < k < len(spectrum) - 1:
            a0, a1, a2 = spectrum[k - 1], spectrum[k], spectrum[k + 1]
            denom = a0 - 2.0 * a1 + a2
            offset = 0.5 * (a0 - a2) / denom if denom != 0.0 else 0.0
        else:
            offset = 0.0

        peak_hz = (k + offset) * SAMPLE_RATE / len(chunk)

        # 2 % is a sixth of the 1/6-octave spacing between steps, so a step that landed on
        # its neighbour could not pass at any frequency.
        error = abs(peak_hz - hz) / hz
        worst = max(worst, error)
        assert error < 0.02, f"step {i}: wanted {hz:.1f} Hz, got {peak_hz:.1f} Hz"

    return worst


def main():
    out_dir = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / "Audiosurf Spectrum Calibration.wav"

    audio = build()
    peak = float(np.max(np.abs(audio)))

    # WAV, and specifically not Vorbis. `.WAV` goes down the same BASS stream path as
    # `.OGG` inside the game (VisMusic Do_SeeIfItIsWMA sets SongFileType = 1 for both),
    # so nothing is gained by encoding - and something is lost: at libsndfile's default
    # quality the Vorbis encoder lowpasses hard enough that the 20 480 Hz step comes back
    # as silence. A calibration signal whose top octave the codec threw away would have
    # been read as the game having no band up there.
    #
    # Written in blocks: handing libsndfile the whole minute in one call overflows the
    # stack inside its writer and takes the interpreter with it, leaving a
    # plausible-looking truncated file behind.
    block = SAMPLE_RATE
    with sf.SoundFile(path, "w", samplerate=SAMPLE_RATE, channels=1,
                      format="WAV", subtype="PCM_16") as handle:
        for start in range(0, len(audio), block):
            handle.write(audio[start:start + block])

    worst = verify(path)

    print(f"wrote {path}")
    print(f"  {len(audio) / SAMPLE_RATE:.1f} s, peak {peak:.3f} ({20 * np.log10(peak):.1f} dBFS)")
    print(f"  verified: all {STEP_COUNT} steps within {worst * 100:.2f} % of their frequency")
    print()
    print("music.lua must carry the same schedule:")
    print(f"    local CAL_LEAD_IN = {LEAD_IN_SECONDS}")
    print(f"    local CAL_STEP    = {STEP_SECONDS}")
    print(f"    local CAL_FIRST_HZ = {FIRST_HZ}")
    print(f"    local CAL_PER_OCTAVE = {STEPS_PER_OCTAVE}")
    print(f"    local CAL_STEPS   = {STEP_COUNT}")
    print()
    print("step schedule (index, start second, Hz):")
    for i in range(STEP_COUNT):
        start = LEAD_IN_SECONDS + i * STEP_SECONDS
        end = "\n" if i % 4 == 3 else "   "
        print(f"  {i:3d} {start:6.2f}s {step_hz(i):9.1f}", end=end)
    print()


if __name__ == "__main__":
    main()
