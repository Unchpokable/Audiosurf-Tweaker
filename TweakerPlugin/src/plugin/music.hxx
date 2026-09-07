#pragma once

// What the playing song is doing right now, in musical terms.
//
// The game already computes all of this. `sounds/VisMusic.cgr` runs every frame under Do_Gameplay,
// asks BASS for an FFT, and publishes the result as ordinary channels - so nothing here hooks BASS
// or touches audio. It reads the graph, exactly like the scripting layer does, and reduces what it
// finds to the handful of numbers a shader can use. See Docs/Internal/reversing-journal-gameplay.md
// §10 for where every value comes from and how the band layout was measured.
//
// **Musical names, not the game's.** The game hands out twelve spectrum bands, and those bands are
// *linear*: 21 FFT bins each, 1808.7 Hz apiece (§10.8, measured with a stepped tone rather than
// guessed). Band 0 alone therefore covers 0-1.8 kHz - every fundamental in the music - and nine of
// the twelve sit above 5.4 kHz where only cymbals and hiss live. Publishing them as `band[k]` would
// have frozen another program's FFT layout into this one's API. Four musical groups do not, which
// is what makes the source replaceable: if this ever grows its own FFT through bass.dll, nothing
// above this header changes.
//
// **Every value is sanitised, not trusted.** The game's own loudness is a division by
// `Highway::maxIntensity`, and that channel is zero until the song has been analysed - so the
// engine's own arithmetic produces an infinity or a NaN at least once per run. A NaN reaching a
// shader constant paints the sky black in silence. Nothing leaves here without being checked for
// finiteness and clamped.
namespace tw::plugin::music
{
// The rungs of the slow ladder, as one-pole time constants in seconds.
//
// Four rather than one because "how loud is the music" has no single answer - it depends entirely on
// how long you look. Half a second follows bars; ten seconds follows whether this is a verse or a
// chorus. An effect that wants to breathe with the piece and an effect that wants to hit on the beat
// are asking for different numbers, and neither can be derived from the other by a shader, which has
// no state to filter with.
//
// Geometric, roughly a factor of three apart. Rungs closer than that are not visibly different from
// each other, and a ladder whose steps look alike is a wider API for the same one value - the first
// draft of this had 0.5, 1.0 and 1.5, and the top two would have tracked each other almost exactly.
//
// A one-pole filter reaches 63 % of a step in its time constant and is most of the way there by
// three of them, so read these as "the last second and a half or so", not as a boxcar window.
inline constexpr int k_slow_rungs = 4;
inline constexpr float k_slow_seconds[k_slow_rungs] = { 0.5f, 1.5f, 4.0f, 10.0f };

// One frame's worth. Every field is finite; the ranges below always hold.
//
// **Everything published here is already smoothed.** A pixel shader has no state: it cannot average,
// cannot remember the previous frame, and cannot low-pass anything. Handing it a raw per-frame value
// is handing it noise and no way to remove it, so the filtering happens here or it does not happen.
struct frame {
    // The game's own loudness, normalised against the loudest moment of *this* song (found during
    // the pre-ride analysis) and clamped to 0..1. Raw, per frame.
    //
    // **Not usable as "how loud is the music".** It is the sum of all twelve bands, and nine of
    // those sit above 5.4 kHz (§10.8) - so broadband high content lands in nine accumulators at once
    // while a bass note reaches one. Driven in game, it twitched on sounds that were not audible and
    // sat still under a sustained bass line.
    //
    // How much of that is the band arithmetic and how much was the onset detector firing on the same
    // signal is **not** separated: both were changed together. `music.lua` now prints this and
    // `body` side by side, which is what would show it.
    //
    // Kept because it is the game's own number, used by 179 of its own consumers, and because
    // something wanting the total should not have to recompute it. Use `body` instead.
    float level {};

    // How loud the music is, as a listener would mean it: everything below 3.6 kHz - bands 0 and 1 -
    // smoothed with a fast attack and a slow release.
    //
    // That cut is not a taste setting. Below 3.6 kHz is where the fundamentals live: bass, voice,
    // strings, guitars. Above it is harmonics, and above 5.4 kHz there is nothing but cymbals,
    // breath and distortion. A sustained bass note holds this up for as long as it sounds, which is
    // the behaviour `level` cannot produce.
    //
    // **This is the one to drive things with.**
    float body {};

    // 1.0 the frame an onset is detected, decaying afterwards. For flashes and kicks.
    //
    // Detected on `body`, not on `level`: an onset detector watching the total fires on hi-hats,
    // which are almost pure transient, and stays silent through a kick drum, which is almost none.
    float onset {};

    // Seconds since the last onset, and 0..1 through the song.
    float since_onset {};
    float progress {};

    // Position and length, in seconds.
    float seconds {};
    float length {};

    // The spectrum, folded into four groups, normalised the same way `level` is so they share one
    // scale, and smoothed like `body` for the reason at the top of this struct. Boundaries are the
    // measured band edges, not round numbers: bands are 1808.7 Hz wide and there is nowhere else to
    // cut.
    //
    // `low + mid` is what `body` is made of; the two above it are there for effects that genuinely
    // want the top end - a shimmer on cymbals - rather than for measuring loudness.
    float low {};  // band 0:     0 .. 1.8 kHz - bass, voice, strings, guitars. Everything's fundamentals.
    float mid {};  // band 1:   1.8 .. 3.6 kHz - upper harmonics, presence
    float high {}; // band 2:   3.6 .. 5.4 kHz - brilliance
    float air {};  // bands 3-11: 5.4 kHz up   - cymbals, breath, hiss, distortion

    // The same body, held over progressively longer windows - see k_slow_seconds.
    //
    // What `body` cannot do. `body` answers "how loud is it right now", and on dense, many-voiced
    // music the answer changes every note: an effect driven by it reads as flicker even when it is
    // technically following the audio. These answer "how loud has it been", which is what an effect
    // that should swell and subside with the piece is actually asking.
    //
    // Symmetric, unlike `body`: rising and falling at the same rate is the whole point. The fast
    // attack that makes `body` land on a beat is exactly what would put a step back into these.
    //
    // Each is filtered from the raw body, not from the rung below it. A cascade would lag by the sum
    // of its stages and none of the constants above would mean what it says.
    float slow[k_slow_rungs] {};

    // 1 when the values above came from a resolved graph with a sane normaliser, 0 otherwise -
    // between songs, in menus the group is not loaded for, before the engine pointer is captured.
    // A shader that multiplies by this fades out instead of freezing on the last frame it saw.
    float valid {};
};

// Samples the graph and advances the smoothing. Call once per frame from the game's own thread -
// the same thread the channel graph runs on, which is the render thread the draw hooks fire on.
//
// `dt` is the frame time in seconds; it drives the attack/release and the onset decay, so passing a
// bogus one makes the output frame-rate dependent rather than merely wrong.
//
// Cheap: a dozen channel reads, no allocation, and it gives up immediately when the group is not
// loaded. Re-resolving after a miss is rate limited, because a name lookup is a linear scan of the
// group and a miss is the common case in menus.
void sample(float dt) noexcept;

// The most recent sample. Never stale-unsafe: `valid` is 0 when the last sample found nothing.
[[nodiscard]] const frame& current() noexcept;

// Drops the resolved channel pointers. Call when the graph may have been torn down under us - a
// group unloads when the game leaves a ride, and a pointer into it does not survive that.
void invalidate() noexcept;
} // namespace tw::plugin::music
