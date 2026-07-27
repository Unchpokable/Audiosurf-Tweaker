#include "pch.hxx"

#include "framework/wndproc_hub.hxx"

#include "plugin/diagnostics.hxx"

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
        return g_hooked_unicode ? ::CallWindowProcW(reinterpret_cast<WNDPROC>(g_original_wndproc), hwnd, msg, wparam, lparam)
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

    const LONG_PTR previous = g_hooked_unicode ? ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hub_wndproc))
                                               : ::SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hub_wndproc));

    g_original_wndproc = reinterpret_cast<wndproc_fn>(previous);
    g_hooked_hwnd = hwnd;

    TW_LOG_INFO("wndproc: hooked hwnd={} (unicode={}, previous proc={}, {} msg subs, {} catch-all subs)",
        static_cast<const void*>(hwnd),
        g_hooked_unicode,
        reinterpret_cast<const void*>(g_original_wndproc),
        g_msg_subscribers.size(),
        g_catch_all_subscribers.size());
}

void uninstall() noexcept
{
    if(g_hooked_hwnd == nullptr) {
        return;
    }

    if(g_original_wndproc != nullptr) {
        // Only unhook if we are still the window's current proc. Anyone who subclassed after us
        // (Steam overlay, RivaTuner, another injected tool) chained to hub_wndproc and holds it as
        // *their* "original" - blindly writing our saved pointer back would erase them from the
        // chain and leave them calling into a proc nobody points at anymore. Leaving hub_wndproc
        // installed is the safe direction: it keeps forwarding, and the plugin has no unload path
        // that would turn it into a dangling address (see CLAUDE.md).
        const LONG_PTR current = g_hooked_unicode ? ::GetWindowLongPtrW(g_hooked_hwnd, GWLP_WNDPROC)
                                                  : ::GetWindowLongPtrA(g_hooked_hwnd, GWLP_WNDPROC);

        if(current != reinterpret_cast<LONG_PTR>(&hub_wndproc)) {
            TW_LOG_WARNING("wndproc: hwnd={} was subclassed by someone else after us (current proc={}), leaving our hook in place",
                static_cast<const void*>(g_hooked_hwnd),
                reinterpret_cast<const void*>(current));
        }
        else if(g_hooked_unicode) {
            // Restore through the same A/W family used to install, so the window's proc-encoding
            // flag ends up exactly as it started.
            ::SetWindowLongPtrW(g_hooked_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
            TW_LOG_INFO("wndproc: restored original proc on hwnd={}", static_cast<const void*>(g_hooked_hwnd));
        }
        else {
            ::SetWindowLongPtrA(g_hooked_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
            TW_LOG_INFO("wndproc: restored original proc on hwnd={}", static_cast<const void*>(g_hooked_hwnd));
        }
    }

    g_hooked_hwnd = nullptr;
    g_original_wndproc = nullptr;
}
} // namespace tw::framework::wndproc
