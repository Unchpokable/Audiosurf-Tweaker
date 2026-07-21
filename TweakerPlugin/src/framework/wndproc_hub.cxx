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

// The hooked window's native ANSI/Unicode-ness, captured at install() time. Everything that touches
// the window proc (SetWindowLongPtr to install/restore, and the call into the original below) has
// to use the matching A/W variant - see install() for why.
bool g_hooked_unicode = false;

// Forwards to the previously-installed proc via CallWindowProc rather than calling the raw pointer:
// CallWindowProc transparently bridges ANSI<->Unicode message translation for that proc and handles
// the case where it's a subclass/thunk handle rather than a plain function address. Falls back to
// DefWindowProc when nothing was there before.
LRESULT call_original_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(g_original_wndproc != nullptr) {
        return g_hooked_unicode
            ? ::CallWindowProcW(reinterpret_cast<WNDPROC>(g_original_wndproc), hwnd, msg, wparam, lparam)
            : ::CallWindowProcA(reinterpret_cast<WNDPROC>(g_original_wndproc), hwnd, msg, wparam, lparam);
    }

    return g_hooked_unicode ? ::DefWindowProcW(hwnd, msg, wparam, lparam) : ::DefWindowProcA(hwnd, msg, wparam, lparam);
}

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

    return call_original_wndproc(hwnd, msg, wparam, lparam);
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

    // Match the window's native ANSI/Unicode-ness. Installing a Unicode proc (SetWindowLongPtrW)
    // onto a window whose class was registered ANSI flips the window's proc-encoding flag, and
    // Windows then silently transcodes every text-bearing message (WM_CHAR, WM_SETTEXT, ...) as it
    // crosses between our proc and the game's - corrupting them. Quest3D registers its window ANSI,
    // so this must be queried per-window, not assumed.
    g_hooked_unicode = ::IsWindowUnicode(hwnd) != FALSE;

    const LONG_PTR previous = g_hooked_unicode
        ? ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hub_wndproc))
        : ::SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hub_wndproc));

    g_original_wndproc = reinterpret_cast<wndproc_fn>(previous);
    g_hooked_hwnd = hwnd;
}

void uninstall() noexcept
{
    if(g_hooked_hwnd == nullptr) {
        return;
    }

    if(g_original_wndproc != nullptr) {
        // Restore through the same A/W family used to install, so the window's proc-encoding flag
        // ends up exactly as it started.
        if(g_hooked_unicode) {
            ::SetWindowLongPtrW(g_hooked_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
        }
        else {
            ::SetWindowLongPtrA(g_hooked_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
        }
    }

    g_hooked_hwnd = nullptr;
    g_original_wndproc = nullptr;
}
} // namespace tw::framework::wndproc
