// No TweakerPlugin PCH here - part of tweaker_ui, shared with smoke_test (same as ui/widgets/).
#include "ui/kit/anim.hxx"

namespace
{
tweeny::tween<float> make_tween(float from, float to, tw::ui::theme::timing timing)
{
    auto tween = tweeny::from(from).to(to).during(timing.duration_ms);
    switch(timing.curve) {
        case tw::ui::theme::easing::quadratic_out:
            return tween.via(tweeny::easing::quadraticOut);
        case tw::ui::theme::easing::cubic_out:
        default:
            return tween.via(tweeny::easing::cubicOut);
    }
}
} // namespace

namespace tw::ui::kit::anim
{
channel::channel(theme::timing timing, float initial) noexcept : m_timing(timing), m_value(initial), m_target(initial)
{
}

void channel::set(float target)
{
    if(target == m_target) {
        return;
    }

    m_target = target;
    m_tween = make_tween(m_value, target, m_timing);
    m_moving = true;
}

void channel::set(bool on)
{
    set(on ? 1.f : 0.f);
}

void channel::restart(float from, float to)
{
    m_value = from;
    m_target = to;
    m_tween = make_tween(from, to, m_timing);
    m_moving = true;
}

void channel::snap(float value) noexcept
{
    // The tween is left as it is: m_moving is what step() looks at, and replacing the tween here would
    // make snap() able to allocate.
    m_value = value;
    m_target = value;
    m_moving = false;
}

float channel::step()
{
    // The clock is read even when nothing moves. dt_ms() carries the sub-millisecond remainder from one
    // frame it is asked on to the next, so the frames it is *not* asked on drop out of the carry: at
    // 60 Hz the 16/17/17 ms cycle shifts, and the first step of the next curve comes out a millisecond
    // different. Every widget used to read the clock on every update for exactly this reason, and the
    // snapshot stand caught the version that did not (overlay-widgets-history.md, §2).
    const std::int32_t dt = dt_ms();

    if(m_moving) {
        if(m_tween.isFinished()) {
            m_moving = false;
        }
        else {
            m_value = m_tween.step(dt);
        }
    }

    return m_value;
}

float channel::value() const noexcept
{
    return m_value;
}

float channel::target() const noexcept
{
    return m_target;
}

bool channel::moving() const noexcept
{
    return m_moving && !m_tween.isFinished();
}
} // namespace tw::ui::kit::anim
