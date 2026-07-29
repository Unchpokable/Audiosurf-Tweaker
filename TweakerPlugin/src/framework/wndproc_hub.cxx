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
    if(hwnd == nullptr) {
        return;
    }

    // hub_wndproc is already somewhere in this window's proc chain - not necessarily as its topmost
    // proc, see uninstall(). Re-running SetWindowLongPtr here would capture whoever subclassed
    // *above* us as our new "original" and start calling into them, while they still hold
    // hub_wndproc as *their* original: a closed WndProc cycle that stack-overflows on the very next
    // message. That is not a corner case - going exclusive-fullscreen makes d3d9.dll subclass the
    // focus window, and the d3d9 device swap that follows drives unbind_device -> bind_device, i.e.
    // uninstall() then install() on the exact same HWND.
    if(hwnd == g_hooked_hwnd) {
        return;
    }

    if(g_hooked_hwnd != nullptr) {
        uninstall();

        // uninstall() leaves g_hooked_hwnd set when it could not take us back out of the chain.
        // We are about to repoint our single g_original_wndproc at the new window, so messages
        // still reaching us through the old window's chain will be forwarded to the wrong proc.
        // Nothing sane can be done about it from a single-window hub; record it and move on.
        if(g_hooked_hwnd != nullptr) {
            TW_LOG_WARNING("wndproc: moving hook {} -> {} while still stuck in the old window's chain; its forwarding is now wrong",
                static_cast<const void*>(g_hooked_hwnd),
                static_cast<const void*>(hwnd));
        }
    }

    // Match the window's native ANSI/Unicode-ness. Installing a Unicode proc (SetWindowLongPtrW)
    // onto a window whose class was registered ANSI flips the window's proc-encoding flag, and
    // Windows then silently transcodes every text-bearing message (WM_CHAR, WM_SETTEXT, ...) as it
    // crosses between our proc and the game's - corrupting them. Quest3D registers its window ANSI,
    // so this must be queried per-window, not assumed.
    g_hooked_unicode = ::IsWindowUnicode(hwnd) != FALSE;

    const LONG_PTR previous = g_hooked_unicode ? ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hub_wndproc))
                                               : ::SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&hub_wndproc));

    // Last line of defence against forwarding to ourselves: the guard above only covers loops we
    // can see in our own bookkeeping, this one catches a stale hub_wndproc left on the window by
    // bookkeeping that already went wrong. Keeping it as the proc is harmless - we are it - but
    // storing it as the original would recurse, so chain to DefWindowProc instead.
    if(previous == reinterpret_cast<LONG_PTR>(&hub_wndproc)) {
        TW_LOG_WARNING("wndproc: hwnd={} already had hub_wndproc installed, forwarding to DefWindowProc to avoid a proc cycle",
            static_cast<const void*>(hwnd));
        g_original_wndproc = nullptr;
    }
    else {
        g_original_wndproc = reinterpret_cast<wndproc_fn>(previous);
    }

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

    // Only unhook if we are still the window's current proc. Anyone who subclassed after us (Steam
    // overlay, RivaTuner, d3d9.dll on an exclusive-fullscreen device) chained to hub_wndproc and
    // holds it as *their* "original" - blindly writing our saved pointer back would erase them from
    // the chain and leave them calling into a proc nobody points at anymore. Leaving hub_wndproc
    // installed is the safe direction: it keeps forwarding, and the plugin has no unload path that
    // would turn it into a dangling address (see CLAUDE.md).
    //
    // Note this is read through the same A/W family we installed with, so it round-trips as the raw
    // function address rather than an opaque thunk handle - the comparison below is meaningful.
    const LONG_PTR current = g_hooked_unicode ? ::GetWindowLongPtrW(g_hooked_hwnd, GWLP_WNDPROC)
                                              : ::GetWindowLongPtrA(g_hooked_hwnd, GWLP_WNDPROC);

    if(current != reinterpret_cast<LONG_PTR>(&hub_wndproc)) {
        // Bail out *without* clearing the bookkeeping. hub_wndproc is still live in this window's
        // chain and is still being called, so it must keep forwarding to g_original_wndproc, and
        // install() must keep treating this HWND as already hooked. Clearing here was what turned
        // the next install() on the same window into a hub_wndproc -> subclasser -> hub_wndproc
        // cycle (unbounded recursion, stack overflow on the first message after going fullscreen),
        // and, on its own, silently rerouted every message to DefWindowProc.
        TW_LOG_WARNING("wndproc: hwnd={} was subclassed by someone else after us (current proc={}), leaving our hook in place",
            static_cast<const void*>(g_hooked_hwnd),
            reinterpret_cast<const void*>(current));
        return;
    }

    // We are the topmost proc, so we can safely drop out. Restore through the same A/W family used
    // to install, so the window's proc-encoding flag ends up exactly as it started. A null original
    // means install() never captured one; hand the window back to DefWindowProc rather than leaving
    // hub_wndproc behind as an untracked proc that the next install() would have to untangle.
    const LONG_PTR restore = g_original_wndproc != nullptr
        ? reinterpret_cast<LONG_PTR>(g_original_wndproc)
        : reinterpret_cast<LONG_PTR>(g_hooked_unicode ? &::DefWindowProcW : &::DefWindowProcA);

    if(g_hooked_unicode) {
        ::SetWindowLongPtrW(g_hooked_hwnd, GWLP_WNDPROC, restore);
    }
    else {
        ::SetWindowLongPtrA(g_hooked_hwnd, GWLP_WNDPROC, restore);
    }

    TW_LOG_INFO("wndproc: restored proc {} on hwnd={}", reinterpret_cast<const void*>(restore), static_cast<const void*>(g_hooked_hwnd));

    g_hooked_hwnd = nullptr;
    g_original_wndproc = nullptr;
}
} // namespace tw::framework::wndproc
