#include <vd.hxx>

#include "window/native_window.hxx"
#include "wnd_handle.hxx"

namespace
{
as::wnd::wnd_handle window {};
as::wnd::wndproc_handler registered_handler {};
} // namespace

namespace
{
LRESULT WINAPI internal_wndproc_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if(registered_handler) {
        auto code = registered_handler(hwnd, msg, wparam, lparam);
        return code;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}
} // namespace

namespace as::wnd
{
void initialize(as::raw_sys_const_string title)
{
    vd::require(!window.valid(), "as::wnd::initialize: window already initialized");
    vd::require(std::wcslen(title) > 0, "as::wnd::initialize: title cannot be empty");

    window = wnd_handle::create_new(nullptr, title, 0, 0);
}

void shutdown()
{
    vd::require(window.valid(), "as::wnd::shutdown: window not initialized");
    window.release();
}
} // namespace as::wnd

namespace as::wnd
{
void set_wndproc_handler(wndproc_handler handler)
{
    registered_handler = handler;
}

void remove_wndproc_handler()
{
    registered_handler = nullptr;
}
} // namespace as::wnd

namespace as::wnd
{
wnd_handle& get_window()
{
    return window;
}
} // namespace as::wnd
