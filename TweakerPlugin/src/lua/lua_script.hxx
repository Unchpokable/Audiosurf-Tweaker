#pragma once

// One script: the file, what its header says about it, and how it is doing.
//
// **The unit of failure is this, not the layer.** Before Ф4 of lua-engine-fix-roadmap.md a script
// that threw once took every script's on_frame down with it, because there was one pcall per
// dispatcher and one latch behind it. Now each script carries its own health - its own error budget,
// its own time budget, its own suspension - and one script's bad frame is nobody else's business.
//
// Everything here runs on the engine/render thread and only there (lua-scripting.md §7).
namespace tw::lua
{
// Which callback a script is running. Shared with the prelude (the K_* constants in lua_prelude.cxx)
// and passed through tw_script_enter/leave, so the numbering is ABI: append, never renumber.
enum class callback : int {
    frame = 0,
    tick = 1,
    post_tick = 2,
    call = 3,
    ready = 4,
    state = 5,
    group = 6,
    unload = 7,
};

inline constexpr int k_callback_count = 8;

[[nodiscard]] const char* callback_name(callback kind) noexcept;

// What a script's row says it is doing. Derived, never stored: every input already lives somewhere
// with its own lifetime, and a stored copy is one more thing to keep in step.
enum class script_state {
    off,       // switched off by the user
    failed,    // the file did not load - a syntax error, or its body threw
    suspended, // it ran out of error or time budget; its hooks are inert until Resume or Reload
    waiting,   // running, but held: the game is not up, or a hook's group is not loaded yet
    running,
};

[[nodiscard]] const char* state_name(script_state state) noexcept;

struct script_health {
    bool suspended = false;
    std::string reason; // one line: why it was suspended

    // The error budget: the frames the last few errors happened on, as a ring. Full and all inside
    // the window means the script is failing faster than it is working.
    std::array<std::uint32_t, 5> error_frames {};
    int error_head = 0;
    int error_fill = 0;
    std::uint32_t errors = 0; // since the last (re)load, for the tab

    // The time budget. `pending` collects QPC ticks per callback kind until the scheduler folds them
    // into the moving averages once per engine frame; `average_ms` is per kind, `total_ms` their
    // sum, and `over_hard` counts consecutive frames the total has sat above the hard limit.
    std::array<std::int64_t, k_callback_count> pending {};
    std::array<float, k_callback_count> average_ms {};
    float total_ms = 0.f;
    int over_hard = 0;
};

struct script {
    int id = -1; // stable for the session, equal to its index in the registry; also the owner id
    std::filesystem::path path;

    std::string file;
    std::string name;
    std::string author;
    std::string version;
    std::string description;

    bool enabled = false;
    bool failed = false; // the last load attempt errored - `error` says how
    std::string error;   // first line only; the traceback is in its diagnostics

    script_health health;
};

// Reads the `-- @key value` header annotations without executing anything.
//
// Executing is exactly what must not happen here: the tab lists scripts the user has turned off, and
// running one to find out what it calls itself would defeat the point of having turned it off.
void read_header(script& entry) noexcept;

// Back to a clean slate: nothing suspended, no errors counted, no time on the clock. What loading a
// script from disk means for its health.
void reset_health(script& entry) noexcept;
} // namespace tw::lua
