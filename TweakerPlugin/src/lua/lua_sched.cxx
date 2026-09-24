#include "pch.hxx"

#include "lua/lua_sched.hxx"

#include "engine/engine_control.hxx"
#include "engine/engine_frame.hxx"
#include "engine/engine_groups.hxx"
#include "engine/engine_state.hxx"

#include "lua/api/api_hooks.hxx"
#include "lua/lua_diag.hxx"
#include "lua/lua_registry.hxx"
#include "lua/lua_script.hxx"
#include "lua/lua_vm.hxx"

#include "ui/plugins/static/pins.hxx"

namespace
{
using tw::lua::callback;

// The error budget: this many errors (the size of script_health::error_frames) inside this many
// engine frames suspends a script. 300 frames is five seconds at 60 Hz - long enough that a script
// which fails on the odd frame keeps running, short enough that one failing every frame is stopped
// within a tenth of a second.
constexpr std::uint32_t k_error_window_frames = 300;

// The time budget, per script, in milliseconds of callbacks per engine frame.
constexpr float k_soft_ms = 0.5f;
constexpr float k_hard_ms = 4.f;
constexpr int k_hard_frames = 120;

// Weight of the newest frame in the moving averages. 1/16 settles within a few dozen frames, so the
// hard limit's 120 frames measure a sustained cost rather than the average catching up.
constexpr float k_average_weight = 1.f / 16.f;

// A dispatcher - the prelude function, not any script's handler - failed. Per dispatcher, so a bug in
// one does not silence the others. Cleared by rearm() when the set of running scripts changes.
enum dispatcher : int {
    d_frame,
    d_tick,
    d_post_tick,
    d_call,
    d_state,
    d_graph,
    d_count,
};

std::array<bool, d_count> g_broken {};

// What the lifecycle dispatchers have already told the scripts about. Both are revisions, not
// states: comparing a counter is one load and one branch, and it cannot miss a transition that came
// and went inside one frame.
std::uint32_t g_seen_state_revision = 0;
std::uint32_t g_seen_graph_revision = 0;

// tw.frame: overlay frames counted here for the session where the spine never ticks, and the
// monotonic value actually handed out.
std::uint32_t g_draw_frames = 0;
std::uint32_t g_frame = 0;

// The clock under enter()/leave(). A stack rather than one slot, because nothing forbids a callback
// from causing another one - it does not happen today, and when it does the inner call should not
// corrupt the outer one's measurement. Deeper than this is not measured at all rather than wrongly.
constexpr int k_max_depth = 8;
std::array<std::int64_t, k_max_depth> g_started {};
int g_depth = 0;

double g_ms_per_tick = 0.0;

[[nodiscard]] std::int64_t now_ticks() noexcept
{
    LARGE_INTEGER now {};
    ::QueryPerformanceCounter(&now);
    return now.QuadPart;
}

std::string_view first_line(std::string_view text) noexcept
{
    const std::size_t end = text.find_first_of("\r\n");
    return end == std::string_view::npos ? text : text.substr(0, end);
}

void suspend(tw::lua::script& script, std::string reason) noexcept
{
    tw::lua::script_health& health = script.health;

    health.suspended = true;
    health.reason = std::move(reason);
    health.over_hard = 0;

    tw::lua::diag::report(script.id, tw::lua::diag::level::fatal, "suspended", "suspended - " + health.reason);
}

void count_error(tw::lua::script& script, std::string_view error) noexcept
{
    tw::lua::script_health& health = script.health;
    const std::uint32_t now = tw::lua::sched::frame();
    const int capacity = static_cast<int>(health.error_frames.size());

    ++health.errors;
    health.error_frames[static_cast<std::size_t>(health.error_head)] = now;
    health.error_head = (health.error_head + 1) % capacity;
    health.error_fill = std::min(health.error_fill + 1, capacity);

    if(health.error_fill < capacity || health.suspended) {
        return;
    }

    // The head now points at the oldest of the last `capacity` errors.
    const std::uint32_t oldest = health.error_frames[static_cast<std::size_t>(health.error_head)];
    if(now - oldest <= k_error_window_frames) {
        suspend(script, std::format("{} errors in {} frames, the last: {}", capacity, now - oldest + 1, first_line(error)));
    }
}

// Folds what each script spent since the last fold into its averages - once per engine frame, or
// per overlay frame in a session without the spine - and applies the time limit.
//
// `measured` false throws the frame away: it was loading, or busy loading a run, and every frame
// there is slow for reasons that belong to the game.
void fold(bool measured) noexcept
{
    const int count = tw::lua::registry::count();

    for(int id = 0; id < count; ++id) {
        tw::lua::script* const script = tw::lua::registry::find(id);
        if(script == nullptr) {
            continue;
        }

        tw::lua::script_health& health = script->health;

        if(!measured || !script->enabled || health.suspended) {
            health.pending.fill(0);
            continue;
        }

        std::int64_t total = 0;
        for(std::size_t kind = 0; kind < health.pending.size(); ++kind) {
            const float ms = static_cast<float>(static_cast<double>(health.pending[kind]) * g_ms_per_tick);
            health.average_ms[kind] += (ms - health.average_ms[kind]) * k_average_weight;
            total += health.pending[kind];
            health.pending[kind] = 0;
        }

        const float frame_ms = static_cast<float>(static_cast<double>(total) * g_ms_per_tick);
        health.total_ms += (frame_ms - health.total_ms) * k_average_weight;

        if(health.total_ms <= k_hard_ms) [[likely]] {
            health.over_hard = 0;
            continue;
        }

        if(++health.over_hard >= k_hard_frames) {
            suspend(*script,
                std::format("its callbacks averaged {:.1f} ms per frame for {} frames (the limit is {:.0f} ms)",
                    health.total_ms,
                    k_hard_frames,
                    k_hard_ms));
        }
    }
}

// One protected call into a prelude export. A failure here is the dispatcher's own - every script
// handler inside it is already guarded - so it is filed against the layer and switches that one
// dispatcher off rather than repeating every frame. `latch` is null for the calls that must always
// be attempted (unloading a script).
template<typename TPush>
void run_export(tw::lua::vm::fn which, bool* latch, const char* where, TPush&& push_arguments) noexcept
{
    lua_State* const lua = tw::lua::vm::state();
    if(lua == nullptr || (latch != nullptr && *latch)) [[unlikely]] {
        return;
    }

    const int base = lua_gettop(lua);
    const int handler = tw::lua::vm::push_traceback();

    if(!tw::lua::vm::push(which)) [[unlikely]] {
        lua_settop(lua, base);
        return;
    }

    const int arguments = push_arguments(lua);

    if(lua_pcall(lua, arguments, 0, handler) != 0) [[unlikely]] {
        const char* const message = lua_tostring(lua, -1);
        if(latch != nullptr) {
            *latch = true;
        }
        tw::lua::diag::report(-1, tw::lua::diag::level::fatal, where,
            std::format("the {} dispatcher failed - a bug in the prelude, not in a script: {}", where, message != nullptr ? message : "?"));
    }

    lua_settop(lua, base);
}

// The two dispatchers that run in **every** state, because their only job is to say which state it
// is. Revision-compared rather than state-compared, so a transition that came and went inside one
// frame is still delivered, and so calling this from two places costs nothing.
void dispatch_lifecycle() noexcept
{
    if(const std::uint32_t revision = tw::engine::state::revision(); revision != g_seen_state_revision) [[unlikely]] {
        g_seen_state_revision = revision;
        run_export(tw::lua::vm::fn::dispatch_state, &g_broken[d_state], "on_state", [](lua_State* lua) {
            lua_pushstring(lua, tw::engine::state::current_name());
            return 1;
        });
    }

    if(const std::uint32_t revision = tw::engine::groups::revision(); revision != g_seen_graph_revision) [[unlikely]] {
        g_seen_graph_revision = revision;
        run_export(tw::lua::vm::fn::dispatch_graph, &g_broken[d_graph], "on_group", [](lua_State*) { return 0; });
    }
}
} // namespace

namespace tw::lua::sched
{
void initialize() noexcept
{
    LARGE_INTEGER frequency {};
    ::QueryPerformanceFrequency(&frequency);
    g_ms_per_tick = frequency.QuadPart != 0 ? 1000.0 / static_cast<double>(frequency.QuadPart) : 0.0;

    // In the early load the spine is already installed and already counting frames by now - it holds
    // its subscribers back until framework::ready is published, which happens after the scripting
    // layer comes up. That flag is what makes writing these lists from the startup thread safe while
    // the engine thread reads them (engine_control.hxx).
    //
    // Registered after engine::state::initialize(), which put the group registry and the state
    // machine in front of them: the pre-frame list runs in subscription order, and a script's tick
    // must see this frame's state, not the previous one's.
    tw::engine::control::subscribe_pre(&tick_frame);
    tw::engine::control::subscribe_post(&post_tick_frame);
}

void shutdown() noexcept
{
    g_broken.fill(false);
    g_seen_state_revision = 0;
    g_seen_graph_revision = 0;
    g_depth = 0;
}

void draw_frame() noexcept
{
    if(tw::lua::vm::state() == nullptr) [[unlikely]] {
        return;
    }

    ++g_draw_frames;

    // Also from here, not only from the engine tick: a session where the spine never installed has
    // no engine tick at all, and a script's on_state would then never fire.
    dispatch_lifecycle();

    const bool ready = tw::engine::state::ready();

    if(!tw::engine::frame::started()) [[unlikely]] {
        fold(tw::engine::state::current() == tw::engine::state::phase::ready);
    }

    // **The gate.** Scripts do not draw, do not tick and do not run at all until the game has come
    // up - which is the symptom Ф2 exists to close. The only thing let through is an on_frame that
    // asked for it (before_ready), and the dispatcher filters for that itself.
    //
    // Being held back is a state worth showing rather than hiding, hence the pin - but only when
    // there is actually something being held. A session with no scripts installed has nothing to
    // wait for and says nothing.
    if(ready) [[likely]] {
        tw::ui::plugins::statics::pins::set_status({});
        tw::lua::diag::announce_held();
    }
    else {
        tw::ui::plugins::statics::pins::set_status(tw::lua::registry::loaded_count() > 0 ? "Scripts waiting" : std::string_view {});
    }

    run_export(tw::lua::vm::fn::dispatch_frame, &g_broken[d_frame], "on_frame", [ready](lua_State* lua) {
        lua_pushboolean(lua, ready ? 0 : 1);
        return 1;
    });
}

void tick_frame() noexcept
{
    if(tw::lua::vm::state() == nullptr) [[unlikely]] {
        return;
    }

    // First subscriber on the engine thread after the group registry and the state machine, so what
    // it reads is this frame's state, not the previous one's (see plugin/load.cxx).
    dispatch_lifecycle();

    // The previous frame's cost, now that it is complete. Only `ready` frames count - see the header.
    fold(tw::engine::state::current() == tw::engine::state::phase::ready);

    if(!tw::engine::state::ready()) [[unlikely]] {
        return;
    }

    tw::lua::diag::announce_held();

    run_export(tw::lua::vm::fn::dispatch_tick, &g_broken[d_tick], "on_tick", [](lua_State*) { return 0; });
}

void post_tick_frame() noexcept
{
    if(tw::lua::vm::state() == nullptr || !tw::engine::state::ready()) [[unlikely]] {
        return;
    }

    run_export(tw::lua::vm::fn::dispatch_post_tick, &g_broken[d_post_tick], "on_post_tick", [](lua_State*) { return 0; });
}

bool dispatch_call(int subscription_id) noexcept
{
    lua_State* const lua = tw::lua::vm::state();
    if(lua == nullptr || g_broken[d_call]) [[unlikely]] {
        return true;
    }

    const int base = lua_gettop(lua);
    const int handler = tw::lua::vm::push_traceback();

    if(!tw::lua::vm::push(tw::lua::vm::fn::dispatch_call)) [[unlikely]] {
        lua_settop(lua, base);
        return true;
    }

    lua_pushinteger(lua, subscription_id);

    bool proceed = true;

    if(lua_pcall(lua, 1, 1, handler) != 0) [[unlikely]] {
        const char* const message = lua_tostring(lua, -1);
        g_broken[d_call] = true;
        tw::lua::diag::report(-1, tw::lua::diag::level::fatal, "on_call",
            std::format("the on_call dispatcher failed - a bug in the prelude, not in a script: {}", message != nullptr ? message : "?"));
    }
    else {
        // Only a literal `false` cancels the engine's handler. A handler that returns nothing yields
        // nil here and proceeds - which is what keeps every observer written before suppression
        // existed behaving exactly as it did.
        proceed = !(lua_isboolean(lua, -1) && lua_toboolean(lua, -1) == 0);
    }

    lua_settop(lua, base);
    return proceed;
}

bool enter(int owner, int kind) noexcept
{
    if(owner >= 0) {
        const tw::lua::script* const script = tw::lua::registry::find(owner);
        if(script == nullptr || !script->enabled) [[unlikely]] {
            return false;
        }

        // Unloading is the exception: a suspended script still gets to put back what it changed.
        if(script->health.suspended && kind != static_cast<int>(callback::unload)) [[unlikely]] {
            return false;
        }

        if(g_depth < k_max_depth) [[likely]] {
            g_started[static_cast<std::size_t>(g_depth)] = now_ticks();
        }
        ++g_depth;
    }

    return true;
}

void leave(int owner, int kind, const char* error) noexcept
{
    if(owner < 0) {
        if(error != nullptr) [[unlikely]] {
            tw::lua::diag::report(-1, tw::lua::diag::level::error, tw::lua::callback_name(static_cast<callback>(kind)), error);
        }
        return;
    }

    if(g_depth == 0) [[unlikely]] {
        return;
    }

    --g_depth;
    const std::int64_t elapsed = g_depth < k_max_depth ? now_ticks() - g_started[static_cast<std::size_t>(g_depth)] : 0;

    tw::lua::script* const script = tw::lua::registry::find(owner);
    if(script == nullptr) [[unlikely]] {
        return;
    }

    if(kind >= 0 && kind < tw::lua::k_callback_count) [[likely]] {
        script->health.pending[static_cast<std::size_t>(kind)] += elapsed;
    }

    if(error != nullptr) [[unlikely]] {
        tw::lua::diag::report(owner, tw::lua::diag::level::error, tw::lua::callback_name(static_cast<callback>(kind)), error);
        count_error(*script, error);
    }
}

bool runnable(int owner) noexcept
{
    if(owner < 0) {
        return true;
    }

    const tw::lua::script* const script = tw::lua::registry::find(owner);
    return script != nullptr && script->enabled && !script->health.suspended;
}

bool resume(int owner) noexcept
{
    tw::lua::script* const script = tw::lua::registry::find(owner);
    if(script == nullptr || !script->health.suspended) {
        return false;
    }

    // A clean budget, not the old one: the old one is what suspended it, and would do so again on the
    // first error.
    const std::uint32_t errors = script->health.errors;
    tw::lua::reset_health(*script);
    script->health.errors = errors;

    tw::lua::diag::report(owner, tw::lua::diag::level::info, "resume", "resumed from the Scripts tab");
    return true;
}

void unload(int owner) noexcept
{
    // Lua first, then C. Ordering matters only in one direction: the C records are what the shim
    // holds a pointer to, so they must be the last thing freed. Dropping the Lua callbacks first is
    // harmless - a channel call landing in between finds no handler, and dispatch_call answers
    // "proceed".
    run_export(tw::lua::vm::fn::unload_owner, nullptr, "unload", [owner](lua_State* lua) {
        lua_pushinteger(lua, owner);
        return 1;
    });

    tw::lua::api::unsubscribe_owner(owner);
}

std::uint32_t frame() noexcept
{
    const std::uint32_t candidate = tw::engine::frame::started() ? tw::engine::frame::count() : g_draw_frames;

    // Monotonic clamp, not a max() for its own sake: the handover from the draw counter to the
    // engine counter happens mid-session and the two are unrelated numbers, so without this a script
    // holding `retry_at = tw.frame + 60` would stop retrying until the engine caught up.
    if(candidate > g_frame) {
        g_frame = candidate;
    }

    return g_frame;
}

float soft_budget_ms() noexcept
{
    return k_soft_ms;
}

void rearm() noexcept
{
    g_broken.fill(false);
}
} // namespace tw::lua::sched
