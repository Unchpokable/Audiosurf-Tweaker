#include "pch.hxx"

#include "engine/engine_state.hxx"

#include "engine/engine_control.hxx"
#include "engine/engine_frame.hxx"
#include "engine/engine_groups.hxx"

#include "plugin/boot_log.hxx"
#include "plugin/diagnostics.hxx"

namespace
{
// The project root. `SetNewStartChannel` points the engine here the moment the game's own loader is
// done (reversing-journal-boot.md §2.3), and nothing else in the project is ever the start group
// afterwards. Matched on the bare file name, because the engine stores whatever path the .cgr
// recorded.
constexpr std::string_view k_root_group = "XX_StartHere";

// The game's own loader, the group that is the start group until it hands over (boot journal §2.3).
// Only used to report, once, whether it is still around afterwards - never to decide anything.
constexpr const char* k_loader_group = "start - project loader";

// Defaults for the settle window; overridable from scripts.cfg. 30 frames is roughly half a second
// of a normal menu and 48 ms of the fastest load measured, which is why the millisecond half exists.
constexpr int k_default_settle_frames = 30;
constexpr int k_default_settle_ms = 500;

// How long to wait before declaring `ready` when there is no group registry at all - the degraded
// path where one of HighPoly's group exports was not found. Deliberately much longer than the settle
// window: with nothing to observe, the only honest substitute is "long enough that a cold start has
// certainly finished".
constexpr std::uint64_t k_blind_ready_ms = 4000;

int g_settle_frames = k_default_settle_frames;
int g_settle_ms = k_default_settle_ms;

tw::engine::state::phase g_phase = tw::engine::state::phase::detached;
std::uint32_t g_revision = 0;

// `ready` latches in the sense that matters: once the game has come up, a later burst of group
// activity is `busy`, not a return to `starting`. Going back would mean a script's callbacks stop
// firing every time the player starts a run, which is the opposite of what is wanted.
bool g_was_ready = false;

bool g_blind_warned = false;
std::uint64_t g_first_frame_ms = 0;

std::string g_start_group;

// How fast the graph actually runs while the game is loading.
//
// **This is a measurement, not a diagnostic.** Ф1's log showed 130 engine frames in 208 ms - 625 Hz,
// three times the screen - which is what forced the settle window to be a conjunction of frames and
// milliseconds rather than either alone (§4.1). The explanation offered for it was a guess: that the
// graph runs free wherever it is evaluated without being presented. Nobody has measured it, and a
// Lua probe cannot: scripts do not run before `ready`, which is exactly the window in question.
//
// So the layer measures it itself, once per session, and the lifecycle log carries the answer out of
// every user's machine. The two phases are kept apart because they are different situations: during
// `booting` the game's own loader is driving, during `starting` it has handed over and is still
// assembling itself.
struct phase_span {
    std::uint32_t frames = 0;
    std::uint64_t milliseconds = 0;
};

phase_span g_booting;
phase_span g_starting;

std::uint32_t g_phase_start_frame = 0;
std::uint64_t g_phase_start_ms = 0;

// Frames per second over a span, or 0 when it is too short to divide by. Integer arithmetic: this
// runs once per transition on the engine thread, and a float here would buy nothing.
std::uint32_t hz(const phase_span& span) noexcept
{
    return span.milliseconds > 0 ? static_cast<std::uint32_t>(span.frames * 1000ull / span.milliseconds) : 0;
}

// Whether the engine's start group is the project root, as a three-way answer: the third value is
// what the machine sees when the EngineInterfaceExt identity check failed and there is no signal to
// read at all. Treating "unknown" as "no" would leave such a session stuck in `booting` forever.
enum class root_state {
    no,
    yes,
    unknown,
};

// The last start group seen, and the verdict about it. This runs on the engine thread every frame,
// so the name is only fetched - and the string only touched - when the *pointer* changes, which
// happens twice in a session. Without the cache this would be a heap-touching assign at whatever
// rate the graph runs, and the graph has been measured at 625 Hz.
A3d_ChannelGroup* g_last_start_group = nullptr;
root_state g_last_root_state = root_state::unknown;

root_state start_group_is_root() noexcept
{
    if(!tw::engine::groups::available()) [[unlikely]] {
        return root_state::unknown;
    }

    A3d_ChannelGroup* const group = tw::engine::groups::start_group();
    if(group == g_last_start_group) [[likely]] {
        return g_last_root_state;
    }

    g_last_start_group = group;
    g_last_root_state = root_state::unknown;

    const char* const file = tw::engine::groups::start_group_file();
    if(file == nullptr || file[0] == '\0') {
        g_start_group.clear();
        return g_last_root_state;
    }

    g_start_group.assign(file);

    std::string_view view { file };
    if(const std::size_t slash = view.find_last_of("\\/"); slash != std::string_view::npos) {
        view.remove_prefix(slash + 1);
    }
    if(view.size() > 4 && ::_strnicmp(view.data() + view.size() - 4, ".cgr", 4) == 0) {
        view.remove_suffix(4);
    }

    const bool root = view.size() == k_root_group.size() && ::_strnicmp(view.data(), k_root_group.data(), view.size()) == 0;

    g_last_root_state = root ? root_state::yes : root_state::no;

    return g_last_root_state;
}

// Has the set of loaded groups stood still long enough. Both halves are required: see the header.
bool settled(std::uint64_t now_ms) noexcept
{
    if(tw::engine::groups::count() <= 0) {
        return false;
    }

    const std::uint32_t frames = tw::engine::frame::count() - tw::engine::groups::last_change_frame();
    if(frames < static_cast<std::uint32_t>(g_settle_frames)) {
        return false;
    }

    return now_ms - tw::engine::groups::last_change_ms() >= static_cast<std::uint64_t>(g_settle_ms);
}

void enter(tw::engine::state::phase next) noexcept
{
    if(next == g_phase) {
        return;
    }

    const tw::engine::state::phase previous = g_phase;
    g_phase = next;
    ++g_revision;

    // Close the span that just ended, before anything reads it.
    const std::uint32_t now_frame = tw::engine::frame::count();
    const std::uint64_t now_ms = ::GetTickCount64();
    const phase_span span { now_frame - g_phase_start_frame, now_ms - g_phase_start_ms };

    if(previous == tw::engine::state::phase::booting) {
        g_booting = span;
    }
    else if(previous == tw::engine::state::phase::starting) {
        g_starting = span;
    }

    g_phase_start_frame = now_frame;
    g_phase_start_ms = now_ms;

    TW_LOG_INFO("engine_state: {} -> {} ({} group(s), start group '{}')",
        tw::engine::state::name(previous),
        tw::engine::state::name(next),
        tw::engine::groups::count(),
        g_start_group.empty() ? "?" : g_start_group.c_str());

    if(next == tw::engine::state::phase::ready && previous != tw::engine::state::phase::busy) {
        TW_BOOT_LOG("engine: ready - {} group(s) loaded, start group '{}', {} engine frame(s) in",
            tw::engine::groups::count(),
            g_start_group.empty() ? "?" : g_start_group.c_str(),
            now_frame);

        // The open question Ф2 was told to close, answered in the one place that can see it. On a
        // late injection into a running game both spans are ~0, which is itself the right answer:
        // there was no load to measure.
        TW_BOOT_LOG("engine: graph rate while loading - booting {} frame(s) in {} ms ({} Hz), "
                    "starting {} frame(s) in {} ms ({} Hz)",
            g_booting.frames,
            g_booting.milliseconds,
            hz(g_booting),
            g_starting.frames,
            g_starting.milliseconds,
            hz(g_starting));

        // The other thing nobody had looked at: whether the game's own loader group is still around
        // after it has handed over (reversing-journal-boot.md §10). If it is gone, its progress
        // channel is not a source for anything and the question closes itself; if it stays, it is a
        // second free "loading has finished" signal and a progress bar for the Scripts tab.
        TW_BOOT_LOG("engine: at ready the loader group is {}",
            tw::engine::groups::find(k_loader_group) != nullptr ? "still loaded" : "gone");
    }
}
} // namespace

namespace tw::engine::state
{
bool initialize() noexcept
{
    // Deliberately does **not** require the spine to be installed yet. In the late load it is not:
    // the detours go in after the startup thread has wired everything up, and subscribing before
    // then is exactly what the spine's subscriber lists are built to allow.
    //
    // Order matters and is fixed here rather than left to the caller: the registry has to have
    // rebuilt its roster before the state machine looks at it, or every transition is one frame late
    // and the settle window is measured from the wrong frame.
    groups::initialize();
    control::subscribe_pre(&groups::tick);
    control::subscribe_pre(&tick);

    return groups::available();
}

void tick() noexcept
{
    if(!frame::started()) [[unlikely]] {
        enter(phase::detached);
        return;
    }

    const std::uint64_t now = ::GetTickCount64();
    if(g_first_frame_ms == 0) [[unlikely]] {
        g_first_frame_ms = now;
        g_phase_start_ms = now;
        g_phase_start_frame = frame::count();
    }

    // No registry: nothing to observe, so the only honest thing is to wait out a period long enough
    // that a cold start has certainly finished, say so once, and then get out of the way. Anything
    // cleverer here would be guessing with extra steps.
    if(!groups::available()) [[unlikely]] {
        if(!g_blind_warned) {
            g_blind_warned = true;
            TW_LOG_WARNING("engine_state: no group registry - falling back to a fixed {} ms wait before scripts run", k_blind_ready_ms);
        }

        enter(now - g_first_frame_ms >= k_blind_ready_ms ? phase::ready : phase::booting);
        if(g_phase == phase::ready) {
            g_was_ready = true;
        }
        return;
    }

    if(start_group_is_root() == root_state::no) {
        enter(phase::booting);
        return;
    }

    const bool stable = settled(now);

    if(!g_was_ready) {
        enter(stable ? phase::ready : phase::starting);
        g_was_ready = g_phase == phase::ready;
        return;
    }

    enter(stable ? phase::ready : phase::busy);
}

phase current() noexcept
{
    // **The spine is not installed at all.** Then tick() is never called, nothing observes anything,
    // and the machine would sit in `detached` for the session - which would not be a safe default,
    // it would be scripts silently never running.
    //
    // There is no information to be had here and no point pretending otherwise: say so once, and get
    // out of the way. This is the one path where `ready` does not mean "the game has come up", it
    // means "we cannot tell, and refusing everything is worse than allowing it" - the same call the
    // plugin made before Ф2, for every session.
    if(!control::installed()) [[unlikely]] {
        if(g_phase != phase::ready) {
            g_phase = phase::ready;
            g_was_ready = true;
            ++g_revision;
            TW_LOG_WARNING("engine_state: the frame spine is not installed - nothing to observe, so scripts run ungated");
        }

        return phase::ready;
    }

    return g_phase;
}

const char* name(phase value) noexcept
{
    switch(value) {
    case phase::detached:
        return "detached";
    case phase::booting:
        return "booting";
    case phase::starting:
        return "starting";
    case phase::ready:
        return "ready";
    case phase::busy:
        return "busy";
    }

    return "detached";
}

const char* current_name() noexcept
{
    return name(current());
}

bool ready() noexcept
{
    const phase value = current();
    return value == phase::ready || value == phase::busy;
}

std::uint32_t revision() noexcept
{
    return g_revision;
}

int group_count() noexcept
{
    return groups::count();
}

const char* start_group() noexcept
{
    return g_start_group.c_str();
}

void configure(int frames, int milliseconds) noexcept
{
    g_settle_frames = std::clamp(frames, 0, 100000);
    g_settle_ms = std::clamp(milliseconds, 0, 600000);
}

int settle_frames() noexcept
{
    return g_settle_frames;
}

int settle_ms() noexcept
{
    return g_settle_ms;
}
} // namespace tw::engine::state
