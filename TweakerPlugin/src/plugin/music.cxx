#include "pch.hxx"

#include "plugin/music.hxx"

#include "engine/channel_ref.hxx"
#include "engine/engine_groups.hxx"
#include "engine/family/fam_number.hxx"
#include "engine/family/fam_table.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
namespace channels = tw::engine::channels;

using tw::engine::channel_ref;
using tw::engine::kind;

// The group and the channels inside it. Names are unique within VisMusic for all of these - the one
// exception in that group is `SampleCount`, which exists twice (#10 and #566), and this file does
// not read it: the band count is what the measurement says it is, and a name lookup would have a
// one-in-two chance of returning the wrong channel anyway.
constexpr const char* k_group = "VisMusic";
constexpr const char* k_level = "TotalSpectrumHitThisFrame";
constexpr const char* k_seconds = "RunningTime_Seconds";
constexpr const char* k_length = "SongLength";
constexpr const char* k_spectrum = "New Table: SpectrumHit";
constexpr const char* k_cursor = "SpectrumIndex";

// The normaliser the game divides its own loudness by. It lives in Highway, which is only loaded
// during a ride, and it is zero until the song has been analysed - which is the source of every NaN
// this file exists to catch.
constexpr const char* k_highway_group = "Highway";
constexpr const char* k_max_intensity = "maxIntensity";

// Twelve bands of 21 FFT bins each, measured (reversing-journal-gameplay.md §10.8). Read as a fixed
// count rather than from the game's SampleCount channel: the fold below is written around *these*
// boundaries, so a count that disagreed with them would silently mis-group rather than adapt.
constexpr int k_bands = 12;

// Attack and release, as time constants in seconds. Asymmetric on purpose: the ear hears an onset as
// instant and a decay as gradual, and a symmetric filter fast enough for the first is too fast to
// remove the flicker from the second.
//
// The attack was 20 ms and that was too quick to look at. At 20 ms a single noisy frame is most of
// the way to the target before the next one arrives, so anything driven by it twitches - which is
// the same complaint as driving it from the wrong band, arriving by a different road. 55 ms still
// reads as immediate on a beat and no longer resolves individual frames.
constexpr float k_attack_seconds = 0.055f;
constexpr float k_release_seconds = 0.280f;

// Onset detection, on `body`. `k_flux_ratio` is how far above its own recent average the rise has to
// be, and the hold stops one loud moment from firing every frame it stays loud.
constexpr float k_flux_average_seconds = 0.400f;
constexpr float k_flux_ratio = 1.8f;
constexpr float k_flux_floor = 0.008f;
constexpr float k_onset_hold_seconds = 0.120f;
constexpr float k_onset_decay_seconds = 0.220f;

struct resolved {
    channel_ref level {};
    channel_ref seconds {};
    channel_ref length {};
    channel_ref spectrum {};
    channel_ref cursor {};
    channel_ref max_intensity {};

    [[nodiscard]] bool complete() const noexcept
    {
        // max_intensity is deliberately not required: it lives in a different group with a different
        // lifetime, and everything except the fold's normalisation works without it.
        return level && seconds && length && spectrum && cursor;
    }
};

resolved g_channels {};

// The graph revision the refs above were resolved against (engine_groups::revision()). A miss is the
// normal state in menus and costs a linear scan over the group, so a resolve is attempted once per
// change of the set of loaded groups - which is exactly when the answer can differ - and never in
// between. The same number also retires the refs: Highway is only loaded during a ride, and a ref
// into it that outlived the ride would be a pointer into a destroyed group.
constexpr std::uint32_t k_never = 0xFFFFFFFFu;
std::uint32_t g_resolved_at = k_never;
tw::plugin::music::frame g_frame {};

// Smoothing state, kept here rather than in `frame` so the published struct stays a plain readout.
float g_flux_average {};
float g_previous_body {};
bool g_have_previous {};

// Nothing from the graph is trusted. A channel can hand back an infinity (the game's own division by
// a zero normaliser), a NaN (0/0 in the same place), or simply a number outside the range its name
// promises - and a NaN in a shader constant paints the sky black without a word.
[[nodiscard]] float sane(float value, float low, float high) noexcept
{
    if(!std::isfinite(value)) {
        return low;
    }

    return std::clamp(value, low, high);
}

// A one-pole filter written in terms of a time constant rather than a per-frame coefficient, so the
// smoothing means the same thing at 60 and at 360 fps. The game locks to twice the refresh rate, so
// that difference is not hypothetical.
[[nodiscard]] float approach(float current, float target, float time_constant, float dt) noexcept
{
    if(time_constant <= 0.f || dt <= 0.f) {
        return target;
    }

    const float k = 1.f - std::exp(-dt / time_constant);

    return current + (target - current) * k;
}

// Typed, not generic: slot 17 is GetFloat on a number and six other things elsewhere, so the family
// is checked at resolve and the ref carries it from then on (engine/channel_ref.hxx).
[[nodiscard]] channel_ref find_numeric(const char* group, const char* name) noexcept
{
    channel_ref ref {};
    (void)channels::resolve(group, name, kind::number, ref);
    return ref;
}

bool resolve() noexcept
{
    // Before the revision is stamped, not after: "the engine is not captured yet" says nothing about
    // these channels and must not use up the attempt this revision allows.
    if(!channels::available()) {
        return false;
    }

    const std::uint32_t revision = tw::engine::groups::revision();
    if(revision == g_resolved_at) [[likely]] {
        return g_channels.complete();
    }

    const bool was_complete = g_channels.complete();
    const bool had_normaliser = static_cast<bool>(g_channels.max_intensity);

    g_resolved_at = revision;
    g_channels = {};

    resolved found {};
    found.level = find_numeric(k_group, k_level);
    found.seconds = find_numeric(k_group, k_seconds);
    found.length = find_numeric(k_group, k_length);
    // `Array Value` is the numeric family, which is what makes fam_table::read legal on it.
    found.spectrum = find_numeric(k_group, k_spectrum);
    found.cursor = find_numeric(k_group, k_cursor);

    if(!found.complete()) {
        return false;
    }

    found.max_intensity = find_numeric(k_highway_group, k_max_intensity);

    g_channels = found;

    // Said when it changes, not on every re-resolve: the graph moves twice a run, and a log line per
    // move would say nothing new.
    if(!was_complete || had_normaliser != static_cast<bool>(found.max_intensity)) {
        TW_LOG_INFO("music: VisMusic resolved{}", found.max_intensity ? "" : " (no Highway::maxIntensity yet)");
    }

    return true;
}
} // namespace

namespace tw::plugin::music
{
void invalidate() noexcept
{
    g_channels = {};
    g_frame = {};
    g_resolved_at = k_never;
    g_have_previous = false;
    g_flux_average = 0.f;
}

const frame& current() noexcept
{
    return g_frame;
}

void sample(float dt) noexcept
{
    if(!resolve()) {
        // Fade rather than freeze: a sky driven by these values should settle to its resting state
        // when the music goes away, not hold whatever the last frame happened to contain.
        g_frame.valid = 0.f;
        g_frame.body = approach(g_frame.body, 0.f, k_release_seconds, dt);
        g_frame.onset = approach(g_frame.onset, 0.f, k_onset_decay_seconds, dt);
        g_frame.low = approach(g_frame.low, 0.f, k_release_seconds, dt);
        g_frame.mid = approach(g_frame.mid, 0.f, k_release_seconds, dt);
        g_frame.high = approach(g_frame.high, 0.f, k_release_seconds, dt);
        g_frame.air = approach(g_frame.air, 0.f, k_release_seconds, dt);

        // Down at their own pace, not snapped: a ten-second envelope that jumped to zero the moment
        // a song ended would be a step in whatever it drives, which is the one thing these exist to
        // avoid.
        for(int i = 0; i < tw::plugin::music::k_slow_rungs; ++i) {
            g_frame.slow[i] = approach(g_frame.slow[i], 0.f, tw::plugin::music::k_slow_seconds[i], dt);
        }

        return;
    }

    const float level = sane(tw::engine::fam_number::get(g_channels.level), 0.f, 1.f);
    const float seconds = sane(tw::engine::fam_number::get(g_channels.seconds), 0.f, 100000.f);
    const float length = sane(tw::engine::fam_number::get(g_channels.length), 0.f, 100000.f);

    // The same divisor the game uses on its own loudness, so the groups below land on one scale with
    // `level`. Absent or zero means the song has not been analysed - in which case the groups are
    // published as zero rather than as a division by nothing.
    const float normaliser = g_channels.max_intensity ? sane(tw::engine::fam_number::get(g_channels.max_intensity), 0.f, 1e6f) : 0.f;

    float groups[4] {};

    for(int band = 0; band < k_bands; ++band) {
        const float value = sane(tw::engine::fam_table::read(g_channels.spectrum, g_channels.cursor, static_cast<float>(band)), 0.f, 1e6f);

        // 0, 1, 2, then everything else. Nine bands share the last slot because nine bands share one
        // meaning: above 5.4 kHz there is nothing in music but cymbals, breath and distortion, and
        // splitting that into nine numbers would publish detail that is not there.
        groups[band < 3 ? band : 3] += value;
    }

    g_frame.level = level;
    g_frame.seconds = seconds;
    g_frame.length = length;
    g_frame.progress = length > 0.f ? sane(seconds / length, 0.f, 1.f) : 0.f;

    // Raw group values, before smoothing. Zero when the song has not been analysed yet, rather than
    // a division by a normaliser that is not there.
    float raw[4] {};

    if(normaliser > 0.f) {
        for(int i = 0; i < 4; ++i) {
            raw[i] = sane(groups[i] / normaliser, 0.f, 1.f);
        }
    }

    // Everything published gets the same asymmetric filter, because everything published is read by
    // a shader that cannot filter anything itself.
    const auto smooth = [dt](float current, float target) noexcept {
        return approach(current, target, target > current ? k_attack_seconds : k_release_seconds, dt);
    };

    g_frame.low = smooth(g_frame.low, raw[0]);
    g_frame.mid = smooth(g_frame.mid, raw[1]);
    g_frame.high = smooth(g_frame.high, raw[2]);
    g_frame.air = smooth(g_frame.air, raw[3]);

    // Everything below 3.6 kHz, this frame, before any filtering. Three things are built from it -
    // the fast body, the slow ladder and the onset detector - and each filters it differently, which
    // is why it is computed once here rather than derived from whichever of them ran first.
    const float body_now = std::min(raw[0] + raw[1], 1.f);

    // Smoothed from that raw sum rather than from the two smoothed groups above, so it is one filter
    // rather than a filter of filters - which would lag twice and settle somewhere else.
    g_frame.body = smooth(g_frame.body, body_now);

    // The slow ladder, each rung an independent symmetric filter of the same raw body.
    for(int i = 0; i < tw::plugin::music::k_slow_rungs; ++i) {
        g_frame.slow[i] = approach(g_frame.slow[i], body_now, tw::plugin::music::k_slow_seconds[i], dt);
    }

    // Onset: a rise that is large relative to how much this song has been rising lately. A fixed
    // threshold cannot work here - the level is already normalised per song, but how *jumpy* it is
    // is a property of the genre, and a threshold tuned on drums never fires on strings.
    //
    // On the raw body rather than the smoothed one: the attack filter is what removes the sharp edge
    // an onset detector is looking for, so watching its output would be looking for the thing after
    // deleting it.
    const float flux = g_have_previous ? std::max(0.f, body_now - g_previous_body) : 0.f;
    g_previous_body = body_now;
    g_have_previous = true;

    g_flux_average = approach(g_flux_average, flux, k_flux_average_seconds, dt);

    g_frame.since_onset += dt;
    g_frame.onset = approach(g_frame.onset, 0.f, k_onset_decay_seconds, dt);

    if(g_frame.since_onset >= k_onset_hold_seconds && flux > std::max(k_flux_floor, g_flux_average * k_flux_ratio)) {
        g_frame.onset = 1.f;
        g_frame.since_onset = 0.f;
    }

    g_frame.valid = 1.f;
}
} // namespace tw::plugin::music
