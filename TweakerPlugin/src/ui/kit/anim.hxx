#pragma once

#include <imgui.h>

#include <libtweeny/tweeny.hxx>

#include <cstdint>

#include "ui/theme.hxx"

// Animation as a value type - the layer under every animated thing in the overlay, widget or not.
// Docs/Internal/tweaker-plugin-widgets.md, "kit::anim".
//
// Before this, every widget and every hand-drawn element carried the same three-part recipe: a float
// that is the source of truth, a tween restarted from that float whenever the target changes, and a
// step by the frame's milliseconds while the tween runs. It was written out some twenty times, with the
// durations copied alongside. channel is that recipe once; focus_pair is the other shape that came up,
// the one a virtualized list needs.
namespace tw::ui::kit::anim
{
// Frame delta in whole milliseconds, which is the unit tweeny steps in.
//
// The fractional part is carried into the next frame rather than rounded away, and this matters a
// lot more than it looks: the previous version clamped to a minimum of 1 ms, so above ~1000 FPS
// every frame claimed a full millisecond had passed when a fraction of one had. Every tween then
// ran at (real FPS / 1000)x speed - at the several thousand FPS an uncapped window reaches, a
// 150 ms hover finishes in tens of milliseconds and a 90 ms press is simply not visible. It reads
// exactly like the animations were removed. Carrying the remainder makes the total elapsed time
// exact at any frame rate, and returning 0 on a sub-millisecond frame is correct - the tween just
// does not advance that frame.
//
// Computed once per frame and cached: this is called by every animated element, and a shared carry
// consumed per call would hand the first caller the whole remainder and the rest nothing.
inline std::int32_t dt_ms() noexcept
{
    static int last_frame = -1;
    static std::int32_t frame_dt = 0;
    static float carry = 0.f;

    const int frame = ImGui::GetFrameCount();
    if(frame != last_frame) {
        last_frame = frame;

        const float ms = ImGui::GetIO().DeltaTime * 1000.f + carry;
        frame_dt = static_cast<std::int32_t>(ms);
        carry = ms - static_cast<float>(frame_dt);
    }

    return frame_dt;
}

// One animated float.
//
// The value is the source of truth, not the tween: a tween only exists while the value is moving, and
// a new one always starts from wherever the value is now, so a change of direction mid-way never jumps.
// The value at the end of a curve is whatever the curve produced there - it is not forced onto the
// target, which would differ from it in the last bit and nowhere a human can see, but would make the
// snapshot stand report a change that is not one.
class channel {
public:
    explicit channel(theme::timing timing, float initial = 0.f) noexcept;

    // Heads for `target`. Starts a new curve from the current value only when the target changes - so
    // it can be called every frame with the same target, which is how every caller uses it.
    void set(float target);
    void set(bool on);

    // A new curve from `from` to `to`, whatever the current value and target are. For motion that
    // always starts at the same point: a tab page fading in from nothing, a toast settling into place.
    void restart(float from, float to);

    // Value and target become `value` at once, with no curve. For state that did not come from the
    // user - a widget created already on, a selection pushed in from the host - which should simply be
    // there rather than animate in.
    void snap(float value) noexcept;

    // Advances by this frame's dt_ms() while moving; returns the value either way. Safe to call more
    // than once a frame only in the sense that dt_ms() is the same each time - each call advances.
    float step();

    [[nodiscard]] float value() const noexcept;
    [[nodiscard]] float target() const noexcept;
    [[nodiscard]] bool moving() const noexcept;

private:
    theme::timing m_timing;
    float m_value;
    float m_target;
    bool m_moving = false;
    tweeny::tween<float> m_tween {};
};

// Highlight for a list whose rows have no state of their own.
//
// A virtualized list cannot keep a channel per row: the float attached to row slot 12 animates whatever
// scrolled into that slot, not the row the mouse is on. But at any instant there is exactly one focused
// row - hovered, or selected - so the animation belongs to it, with the row it moved away from kept for
// the length of one crossfade. alpha() answers for any row: rising for the current one, falling for the
// previous one, zero for the rest.
template<typename TKey>
class focus_pair {
public:
    focus_pair(theme::timing timing, TKey none) noexcept : m_channel(timing, 1.f), m_none(none), m_current(none), m_previous(none)
    {
    }

    // Moves focus to `key` with a crossfade from the current one. No-op if it is already there.
    void focus(TKey key)
    {
        if(key == m_current) {
            return;
        }

        m_previous = m_current;
        m_current = key;
        m_channel.restart(0.f, 1.f);
    }

    // Moves focus to `key` with no crossfade: the new one is fully lit and nothing fades out.
    void jump(TKey key) noexcept
    {
        m_current = key;
        m_previous = m_none;
        m_channel.snap(1.f);
    }

    // Nothing focused, nothing fading.
    void reset() noexcept
    {
        jump(m_none);
    }

    // Once per frame, before alpha() is read for that frame's rows.
    void step()
    {
        m_channel.step();
    }

    [[nodiscard]] float alpha(TKey key) const noexcept
    {
        if(key == m_current) {
            return m_channel.value();
        }
        if(key == m_previous) {
            return 1.f - m_channel.value();
        }
        return 0.f;
    }

    [[nodiscard]] TKey current() const noexcept
    {
        return m_current;
    }

private:
    channel m_channel;
    TKey m_none;
    TKey m_current;
    TKey m_previous;
};
} // namespace tw::ui::kit::anim
