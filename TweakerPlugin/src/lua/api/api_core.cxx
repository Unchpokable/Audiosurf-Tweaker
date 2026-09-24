#include "pch.hxx"

#include "lua/api/api_core.hxx"

#include "engine/channel_ref.hxx"
#include "engine/engine_groups.hxx"
#include "engine/engine_state.hxx"

#include "libtweeny/tweeny.hxx"

#include "lua/lua_diag.hxx"
#include "lua/lua_sched.hxx"

#include "plugin/diagnostics.hxx"

#include "ui/widgets/detail/draw.hxx"

#include <imgui.h>

namespace
{
// The easing curves offered to scripts, by index. Appended to, never reordered - an index is part
// of the script-facing ABI, exactly like a font index.
//
// A subset of tweeny's set rather than all 33: these are the ones an overlay animation actually
// reaches for, and a name a script has to guess at is worse than one that is not there.
struct ease_curve {
    const char* name;
    float (*run)(float t);
};

const ease_curve g_ease_curves[] = {
    { "linear", [](float t) { return tweeny::easing::linear.run(t, 0.f, 1.f); } },
    { "quadIn", [](float t) { return tweeny::easing::quadraticIn.run(t, 0.f, 1.f); } },
    { "quadOut", [](float t) { return tweeny::easing::quadraticOut.run(t, 0.f, 1.f); } },
    { "quadInOut", [](float t) { return tweeny::easing::quadraticInOut.run(t, 0.f, 1.f); } },
    { "cubicIn", [](float t) { return tweeny::easing::cubicIn.run(t, 0.f, 1.f); } },
    { "cubicOut", [](float t) { return tweeny::easing::cubicOut.run(t, 0.f, 1.f); } },
    { "cubicInOut", [](float t) { return tweeny::easing::cubicInOut.run(t, 0.f, 1.f); } },
    { "sineIn", [](float t) { return tweeny::easing::sinusoidalIn.run(t, 0.f, 1.f); } },
    { "sineOut", [](float t) { return tweeny::easing::sinusoidalOut.run(t, 0.f, 1.f); } },
    { "sineInOut", [](float t) { return tweeny::easing::sinusoidalInOut.run(t, 0.f, 1.f); } },
    { "expoIn", [](float t) { return tweeny::easing::exponentialIn.run(t, 0.f, 1.f); } },
    { "expoOut", [](float t) { return tweeny::easing::exponentialOut.run(t, 0.f, 1.f); } },
    { "expoInOut", [](float t) { return tweeny::easing::exponentialInOut.run(t, 0.f, 1.f); } },
    { "backIn", [](float t) { return tweeny::easing::backIn.run(t, 0.f, 1.f); } },
    { "backOut", [](float t) { return tweeny::easing::backOut.run(t, 0.f, 1.f); } },
    { "backInOut", [](float t) { return tweeny::easing::backInOut.run(t, 0.f, 1.f); } },
    { "elasticOut", [](float t) { return tweeny::easing::elasticOut.run(t, 0.f, 1.f); } },
    { "bounceOut", [](float t) { return tweeny::easing::bounceOut.run(t, 0.f, 1.f); } },
};
} // namespace

namespace tw::lua::api
{
extern "C" {
int tw_script_enter(int owner, int kind) noexcept
{
    return tw::lua::sched::enter(owner, kind) ? 1 : 0;
}

void tw_script_leave(int owner, int kind, const char* error) noexcept
{
    tw::lua::sched::leave(owner, kind, error);
}

void tw_diag(int owner, int level, const char* where, const char* message) noexcept
{
    if(message == nullptr) {
        return;
    }

    // Clamped rather than trusted: the prelude passes a constant, but a value outside the enum would
    // index past the routing tables in lua_diag.
    const int clamped = std::clamp(level, static_cast<int>(tw::lua::diag::level::pending), static_cast<int>(tw::lua::diag::level::error));
    tw::lua::diag::report(owner, static_cast<tw::lua::diag::level>(clamped), where != nullptr ? where : "", message);
}

void tw_log(const char* message) noexcept
{
    if(message == nullptr) {
        return;
    }

    TW_LOG_INFO("lua: {}", message);
}

void tw_notify(int owner, const char* message) noexcept
{
    if(message == nullptr) {
        return;
    }

    // Copied on the way through (notefeed::push takes a string_view into its own storage), which
    // matters: the pointer is into a Lua string the collector owns.
    tw::lua::diag::notify(owner, message);
}

int tw_engine_ready() noexcept
{
    return tw::engine::channels::available() ? 1 : 0;
}

int tw_can_write() noexcept
{
    return tw::engine::state::ready() ? 1 : 0;
}

int tw_state() noexcept
{
    return static_cast<int>(tw::engine::state::current());
}

const char* tw_state_name() noexcept
{
    return tw::engine::state::current_name();
}

int tw_ready() noexcept
{
    return tw::engine::state::ready() ? 1 : 0;
}

int tw_graph_revision() noexcept
{
    // Degraded path first, and it matters: with no group registry there is no revision to report,
    // and a handle comparing a number that never changes would try to resolve once and then give up
    // for the session. Falling back to a frame-derived value reproduces exactly the old
    // retry-every-60-frames behaviour for a session where the registry could not be built.
    if(!tw::engine::groups::available()) [[unlikely]] {
        return tw_frame() / 60;
    }

    // One number that changes exactly when re-resolving a handle could produce a different answer:
    // a group appeared, or one went away. A script's handle compares it instead of counting frames,
    // which is what turns "rescan the whole group every 60 frames forever" into "rescan when
    // something actually changed" (lua-engine-fix-roadmap.md §5.2).
    return static_cast<int>(tw::engine::groups::revision());
}

int tw_group_count() noexcept
{
    return tw::engine::groups::roster_size();
}

const char* tw_group_name(int index) noexcept
{
    // "<pool name> | <file name>", out of the registry's roster. Module-owned, valid until the next
    // call - Lua copies it into a string on the way through the FFI.
    static std::string buffer;

    A3d_ChannelGroup* const group = tw::engine::groups::roster_at(index);
    if(group == nullptr) {
        buffer.clear();
        return buffer.c_str();
    }

    buffer.assign(tw::engine::groups::pool_name_of(group));
    buffer.append(" | ");
    buffer.append(tw::engine::groups::file_name_of(group));

    return buffer.c_str();
}

int tw_group_loaded(const char* name) noexcept
{
    return tw::engine::groups::find(name) != nullptr ? 1 : 0;
}

int tw_frame() noexcept
{
    return static_cast<int>(tw::lua::sched::frame());
}

float tw_dt() noexcept
{
    if(ImGui::GetCurrentContext() == nullptr) {
        return 0.f;
    }

    // Milliseconds from the overlay's own frame clock rather than ImGui::GetIO().DeltaTime, so a
    // script's animation and an overlay widget's animation cannot disagree about how long a frame
    // was. dt_ms carries the sub-millisecond remainder forward, which is the whole reason it exists.
    return static_cast<float>(tw::ui::widgets::detail::dt_ms()) * 0.001f;
}

float tw_ease(int curve, float t) noexcept
{
    t = std::clamp(t, 0.f, 1.f);

    if(curve < 0 || curve >= static_cast<int>(std::size(g_ease_curves))) {
        return t;
    }

    return g_ease_curves[curve].run(t);
}

int tw_ease_count() noexcept
{
    return static_cast<int>(std::size(g_ease_curves));
}

const char* tw_ease_name(int index) noexcept
{
    if(index < 0 || index >= tw_ease_count()) {
        return nullptr;
    }

    return g_ease_curves[index].name;
}
}
} // namespace tw::lua::api
