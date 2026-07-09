#include <vd.hxx>

#include <array>

#include "window/native_window.hxx"
#include "window/wnd_handle.hxx"

namespace
{
as::wnd::wnd_handle window {};
as::wnd::wndproc_handler registered_handler {};

struct typed_handler_entry final {
    UINT msg_type;
    as::wnd::short_handler handler;
};

constexpr std::size_t max_handlers { 24 };
std::size_t handlers_inserter_idx { 0 };
std::array<typed_handler_entry, max_handlers> specialized_handlers;
} // namespace

namespace
{
LRESULT WINAPI internal_wndproc_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // looks for specialized handlers
    for(std::size_t idx = 0; idx < max_handlers; ++idx) {
        if(auto handler = specialized_handlers[idx]; handler.msg_type == msg) {
            return handler.handler(hwnd, wparam, lparam);
        }
    }

    // no specialized handler - call generic
    if(registered_handler) {
        return registered_handler(hwnd, msg, wparam, lparam);
    }

    // no registered external handler - call default
    return DefWindowProc(hwnd, msg, wparam, lparam);
}
} // namespace

namespace as::wnd
{
void initialize(as::raw_sys_const_string title)
{
    vd::require<asbridge_native_window_failure>(!window.valid(), "as::wnd::initialize: window titled already initialized");
    vd::require<asbridge_native_window_failure>(std::wcslen(title) > 0, "as::wnd::initialize: title cannot be empty");

    window = wnd_handle::create_new(nullptr, title, 0, 0);

    vd::require<asbridge_native_window_failure>(
        window.valid(), "as::wnd::initialize: window titled creation failed! GetLastError={}", ::GetLastError());

    window.set_wndproc(&internal_wndproc_handler);
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
void set_handler_for(msg_type message_type, short_handler handler)
{
    for(std::size_t idx = 0; idx < max_handlers; ++idx) {
        if(specialized_handlers[idx].msg_type == message_type) {
            specialized_handlers[idx] = { .msg_type = message_type, .handler = handler };
            return;
        }
    }

    specialized_handlers[handlers_inserter_idx++] = { .msg_type = message_type, .handler = handler };
}
} // namespace as::wnd

namespace as::wnd
{
wnd_handle& get_window()
{
    return window;
}
} // namespace as::wnd
