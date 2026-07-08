#include <vd.hxx>

#include "window/wnd_handle.hxx"

namespace
{
///@brief Window class shared by every wnd_handle this process creates. It registers a plain
/// DefWindowProc; the real handler is swapped in afterwards via wnd_handle::set_wndproc(), since
/// wndproc_handler already matches the native WNDPROC signature and there is no need for a
/// dedicated class per callback.
constexpr auto k_window_class_name = L"ASBridge::wnd_handle::HiddenWindowClass";

void ensure_class_registered(HINSTANCE instance)
{
    WNDCLASSEXW wc {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ::DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = k_window_class_name;

    if(::RegisterClassExW(&wc) != 0) {
        return;
    }

    // A second (or Nth) wnd_handle::create_new() call hitting an already-registered class is expected.
    vd::require(
        ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "as::wnd::wnd_handle: RegisterClassExW failed, GetLastError={}", ::GetLastError());
}
} // namespace

namespace as::wnd
{
wnd_handle wnd_handle::create_new(wndproc_handler wndproc, as::raw_sys_const_string title, int width, int height)
{
    const HINSTANCE instance = ::GetModuleHandleW(nullptr);
    ensure_class_registered(instance);

    const HWND hwnd = ::CreateWindowExW(0,
        k_window_class_name,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        nullptr);

    vd::require(hwnd != nullptr, "as::wnd::wnd_handle::create_new: CreateWindowExW failed, GetLastError={}", ::GetLastError());

    wnd_handle handle(hwnd);
    if(wndproc != nullptr) {
        vd::require(handle.set_wndproc(wndproc), "as::wnd::wnd_handle::create_new: set_wndproc failed, GetLastError={}", ::GetLastError());
    }
    return handle;
}

wnd_handle::wnd_handle(HWND handle) : m_handle(handle)
{
}

wnd_handle::~wnd_handle()
{
    if(!valid()) {
        return;
    }
    ::DestroyWindow(m_handle);
}

wnd_handle::wnd_handle(wnd_handle&& other) noexcept : m_handle(other.m_handle), m_released_or_moved(other.m_released_or_moved)
{
    other.m_released_or_moved = true;
}

wnd_handle& wnd_handle::operator=(wnd_handle&& other) noexcept
{
    if(this == &other) {
        return *this;
    }

    if(valid()) {
        ::DestroyWindow(m_handle);
    }

    m_handle = other.m_handle;
    m_released_or_moved = other.m_released_or_moved;
    other.m_released_or_moved = true;

    return *this;
}

bool wnd_handle::valid() const noexcept
{
    return !m_released_or_moved && m_handle != nullptr;
}

HWND wnd_handle::native() const noexcept
{
    return m_handle;
}

void wnd_handle::release() noexcept
{
    m_released_or_moved = true;
}

bool wnd_handle::set_wndproc(wndproc_handler wndproc) noexcept
{
    if(!valid() || wndproc == nullptr) {
        return false;
    }

    // SetWindowLongPtrW can legitimately return 0 as the *previous* value; GetLastError() must be
    // cleared beforehand to tell that apart from a real failure (documented Win32 idiom).
    ::SetLastError(0);
    const LONG_PTR previous = ::SetWindowLongPtrW(m_handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndproc));
    return previous != 0 || ::GetLastError() == 0;
}

bool wnd_handle::resize(int width, int height) noexcept
{
    if(!valid()) {
        return false;
    }
    return ::SetWindowPos(m_handle, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
}

bool wnd_handle::rename(as::raw_sys_const_string title) noexcept
{
    if(!valid()) {
        return false;
    }
    return ::SetWindowTextW(m_handle, title) != FALSE;
}
} // namespace as::wnd
