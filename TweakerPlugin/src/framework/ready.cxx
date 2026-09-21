#include "pch.hxx"

#include "framework/ready.hxx"

namespace
{
std::atomic<bool> g_ready { false };
} // namespace

namespace tw::framework::ready
{
void publish() noexcept
{
    g_ready.store(true, std::memory_order_relaxed);
}

bool published() noexcept
{
    return g_ready.load(std::memory_order_relaxed);
}
} // namespace tw::framework::ready
