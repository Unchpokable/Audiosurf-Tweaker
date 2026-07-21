#include "pch.hxx"

#include "framework/wndproc_hub.hxx"

#include <utility>
#include <vector>

namespace
{
using wndproc_fn = LRESULT(__stdcall*)(HWND, UINT, WPARAM, LPARAM);

// Registration counts are small and fixed at init time (a handful of consumers), so plain vectors
// scanned linearly are simpler than anything hash-based and cost nothing meaningful per message.
std::vector<std::pair<UINT, tw::framework::wndproc::handler_fn>> g_msg_subscribers;
std::vector<tw::framework::wndproc::handler_fn> g_catch_all_subscribers;

HWND g_hooked_hwnd = nullptr;
wndproc_fn g_original_wndproc = nullptr;

LRESULT __stdcall hub_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT out_result = 0;

    for(const auto& [subscribed_msg, handler] : g_msg_subscribers) {
        if(subscribed_msg == msg && handler(hwnd, msg, wparam, lparam, out_result)) {
            return out_result;
        }
    }

    for(const auto& handler : g_catch_all_subscribers) {
        if(handler(hwnd, msg, wparam, lparam, out_result)) {
            return out_result;
        }
    }

    if(g_original_wndproc != nullptr) {
        return g_original_wndproc(hwnd, msg, wparam, lparam);
    }

    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}
} // namespace

namespace tw::framework::wndproc
{
void subscribe(UINT msg, handler_fn handler) noexcept
{
    g_msg_subscribers.emplace_back(msg, handler);
}

void subscribe_all(handler_fn handler) noexcept
{
    g_catch_all_subscribers.push_back(handler);
}

void install(HWND hwnd) noexcept
{
    if(hwnd == g_hooked_hwnd) {
        return;
    }

    if(g_hooked_hwnd != nullptr) {
        uninstall();
    }

    g_original_wndproc = reinterpret_cast<wndproc_fn>(::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hub_wndproc)));
    g_hooked_hwnd = hwnd;
}

void uninstall() noexcept
{
    if(g_hooked_hwnd == nullptr) {
        return;
    }

    if(g_original_wndproc != nullptr) {
        ::SetWindowLongPtrW(g_hooked_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
    }

    g_hooked_hwnd = nullptr;
    g_original_wndproc = nullptr;
}
} // namespace tw::framework::wndproc
