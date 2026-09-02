#include "pch.hxx"

#include "plugin/bg_work.hxx"

#include "plugin/diagnostics.hxx"

namespace
{
// Futures of abandoned tasks, waiting to be destroyed somewhere that is allowed to block.
//
// Each entry owns its future and answers "has it finished yet". Destroying the entry destroys the
// future, and that only ever happens once the answer is yes - which is what makes the destruction
// free rather than a wait dressed up as a list.
std::mutex g_mutex;
std::vector<std::function<bool()>> g_retired;

std::atomic<int> g_outstanding { 0 };
} // namespace

namespace tw::plugin::bg_work
{
namespace detail
{
void retire(std::function<bool()> reap)
{
    std::lock_guard<std::mutex> lock { g_mutex };
    g_retired.push_back(std::move(reap));
}

void note_started() noexcept
{
    g_outstanding.fetch_add(1, std::memory_order_relaxed);
}

void note_finished() noexcept
{
    g_outstanding.fetch_sub(1, std::memory_order_relaxed);
}
} // namespace detail

void poll() noexcept
{
    std::lock_guard<std::mutex> lock { g_mutex };

    if(g_retired.empty()) {
        return;
    }

    // erase-remove over a predicate with a side effect, which is worth being explicit about: the
    // predicate *is* the check, and returning true is what allows the entry - and with it the
    // future - to be destroyed here, on a thread where that destruction costs nothing.
    const auto done = std::remove_if(g_retired.begin(), g_retired.end(), [](std::function<bool()>& reap) {
        return reap();
    });

    if(done != g_retired.end()) {
        g_retired.erase(done, g_retired.end());
    }
}

void shutdown() noexcept
{
    // Nothing here can interrupt a running job - D3DCompile takes no cancellation token - so this is
    // a wait by construction. Bounded, because every job is bounded work rather than a loop.
    for(;;) {
        {
            std::lock_guard<std::mutex> lock { g_mutex };

            const auto done = std::remove_if(g_retired.begin(), g_retired.end(), [](std::function<bool()>& reap) {
                return reap();
            });
            g_retired.erase(done, g_retired.end());

            if(g_retired.empty() && g_outstanding.load(std::memory_order_relaxed) == 0) {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds { 5 });
    }

    TW_LOG_INFO("bg_work: stopped");
}

int pending() noexcept
{
    std::lock_guard<std::mutex> lock { g_mutex };

    return g_outstanding.load(std::memory_order_relaxed) + static_cast<int>(g_retired.size());
}
} // namespace tw::plugin::bg_work
