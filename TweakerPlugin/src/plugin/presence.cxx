#include "pch.hxx"

#include "plugin/presence.hxx"

namespace
{
// Never closed - see the header.
HANDLE g_instance_mutex = nullptr;
HANDLE g_ready_event = nullptr;

std::atomic<bool> g_ready_signalled { false };

// Fixed buffer, no allocation: runs inside DllMain.
void format_name(std::span<wchar_t> buffer, std::wstring_view suffix) noexcept
{
    const auto result =
        std::format_to_n(buffer.data(), buffer.size() - 1, L"Local\\AudiosurfTweaker.Overlay.{}{}", ::GetCurrentProcessId(), suffix);
    *result.out = L'\0';
}
} // namespace

namespace tw::plugin::presence
{
claim_result claim() noexcept
{
    std::array<wchar_t, 96> name {};

    format_name(name, L"");
    g_instance_mutex = ::CreateMutexW(nullptr, FALSE, name.data());
    if(g_instance_mutex == nullptr) {
        return claim_result::unavailable;
    }

    if(::GetLastError() == ERROR_ALREADY_EXISTS) {
        ::CloseHandle(g_instance_mutex);
        g_instance_mutex = nullptr;
        return claim_result::already_loaded;
    }

    format_name(name, L".Ready");
    g_ready_event = ::CreateEventW(nullptr, TRUE, FALSE, name.data());

    return claim_result::claimed;
}

void signal_ready() noexcept
{
    if(g_ready_signalled.exchange(true, std::memory_order_relaxed)) {
        return;
    }

    if(g_ready_event != nullptr) {
        ::SetEvent(g_ready_event);
    }
}
} // namespace tw::plugin::presence
