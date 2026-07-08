#pragma once

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#error "This file is only for Windows builds"
#endif

#include "system/string.hxx"

namespace as::wnd
{
using wndproc_handler = LRESULT(CALLBACK*)(HWND, UINT, WPARAM, LPARAM);
using short_handler = LRESULT (*)(HWND, WPARAM, LPARAM); // internal type for ASBridge, handler for special message type
} // namespace as::wnd

namespace as::wnd
{
using msg_type = UINT;
} // namespace as::wnd

namespace as::wnd
{
///@brief RAII owner of a single native HWND: destroys the window in its destructor unless the
/// object has been released() or moved from. Move transfers ownership; the moved-from object is
/// marked non-owning but keeps its m_handle value around (see native()) purely for diagnostics.
class wnd_handle final {
public:
    ///@brief Registers (once per process, idempotently) a shared hidden window class backed by
    /// DefWindowProc, creates a top-level (not shown) window of that class, then installs `wndproc`
    /// via set_wndproc(). Note: CreateWindowExW dispatches WM_NCCREATE/WM_CREATE synchronously
    /// during creation, before wndproc can be installed - those two messages are always seen by
    /// DefWindowProc, never by the handler passed in here. Aborts (vd::require) on any Win32 failure.
    static wnd_handle create_new(wndproc_handler wndproc, as::raw_sys_const_string title, int width, int height);

    ///@brief looks for existing window with title in the system
    static wnd_handle open_existing(as::raw_sys_const_string title);

    explicit wnd_handle() = default;

    ///@brief Adopts an already-created window; this object owns it from now on. Marked explicit so a
    /// raw HWND is never silently turned into an owning wrapper (which would destroy the window the
    /// moment a temporary wnd_handle goes out of scope).
    explicit wnd_handle(HWND handle);

    ~wnd_handle();

    wnd_handle(const wnd_handle&) = delete;
    wnd_handle(wnd_handle&& other) noexcept;

    wnd_handle& operator=(const wnd_handle&) = delete;
    wnd_handle& operator=(wnd_handle&& other) noexcept;

    ///@brief True only while this object currently owns a live window; false after release() or
    /// after being moved from, even though native() may still report a (now stale) handle value.
    [[nodiscard]] bool valid() const noexcept;

    ///@brief The last known HWND, even after release()/move. Does not imply ownership - check
    /// valid() first if that distinction matters.
    [[nodiscard]] HWND native() const noexcept;

    ///@brief Detaches without destroying the window: the destructor becomes a no-op. Use when
    /// ownership has been handed off elsewhere, or the window is already known to be gone.
    void release() noexcept;

    explicit operator HWND() const noexcept
    {
        return m_handle;
    }

    ///@brief Installs a new window procedure via SetWindowLongPtrW(GWLP_WNDPROC, ...). Returns false
    /// if this handle is invalid, wndproc is null, or the WinAPI call itself fails.
    bool set_wndproc(wndproc_handler wndproc) noexcept;
    bool resize(int width, int height) noexcept;
    bool rename(as::raw_sys_const_string title) noexcept;

private:
    HWND m_handle = nullptr;

    bool m_released_or_moved { false };
    bool m_owning { false };
};
} // namespace as::wnd
