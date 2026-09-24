#include "pch.hxx"

#include "engine/engine_frame.hxx"

#include "engine/engine_symbols.hxx"

#include "plugin/quest3d_state.hxx"

namespace
{
// A frame longer than this is a load, a breakpoint or a minimised window, not an animation step.
// Handing that number to a script makes whatever it drives jump; clamping makes it stall for one
// frame instead, which is the lesser of the two.
constexpr float k_max_dt_seconds = 0.25f;

std::atomic<std::uint32_t> g_count { 0 };
std::atomic<float> g_dt { 0.f };

// Engine thread only.
std::int64_t g_last_tick = 0;
std::int64_t g_qpc_frequency = 0;
} // namespace

namespace tw::engine::frame
{
void begin() noexcept
{
    g_count.fetch_add(1, std::memory_order_relaxed);

    if(g_qpc_frequency == 0) [[unlikely]] {
        LARGE_INTEGER frequency {};
        ::QueryPerformanceFrequency(&frequency);
        g_qpc_frequency = frequency.QuadPart;
    }

    LARGE_INTEGER now {};
    ::QueryPerformanceCounter(&now);

    if(g_last_tick != 0 && g_qpc_frequency != 0) {
        const float seconds = static_cast<float>(static_cast<double>(now.QuadPart - g_last_tick) / static_cast<double>(g_qpc_frequency));
        g_dt.store(std::clamp(seconds, 0.f, k_max_dt_seconds), std::memory_order_relaxed);
    }

    g_last_tick = now.QuadPart;
}

std::uint32_t count() noexcept
{
    return g_count.load(std::memory_order_relaxed);
}

bool started() noexcept
{
    return g_count.load(std::memory_order_relaxed) != 0;
}

float dt_seconds() noexcept
{
    return g_dt.load(std::memory_order_relaxed);
}

int tree_count() noexcept
{
    EngineInterface* const engine = tw::plugin::quest3d::g_engine;
    if(engine == nullptr || !symbols::is_ready()) {
        return -1;
    }

    // Direct call to the export rather than through the vtable: the type is known by construction
    // here - this pointer came out of EngineControl::GetEngineInterface(), whose return type says so
    // (reversing-journal-engine.md §2.5).
    const auto get_count = reinterpret_cast<symbols::get_tree_count_fn>(symbols::get().get_tree_calculate_count);
    return get_count(engine, nullptr);
}
} // namespace tw::engine::frame
