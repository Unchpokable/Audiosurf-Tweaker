#pragma once

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#error "This file is only for Windows builds"
#endif

#include <exception>

#include "window/wnd_handle.hxx"

#include "system/string.hxx"

namespace as::wnd
{
class asbridge_native_window_failure final : public std::exception {
public:
    explicit asbridge_native_window_failure(const std::string& what) : m_message(what)
    {
    }

    explicit asbridge_native_window_failure(const char* what) : m_message(std::string(what))
    {
    }

    [[nodiscard]] virtual const char* what() const noexcept override
    {
        return m_message.c_str();
    }

private:
    std::string m_message;
};
} // namespace as::wnd

namespace as::wnd
{
void initialize(as::raw_sys_const_string title);
void shutdown();
} // namespace as::wnd

namespace as::wnd
{
void set_wndproc_handler(wndproc_handler handler);
void remove_wndproc_handler();
} // namespace as::wnd

namespace as::wnd
{
void set_handler_for(msg_type message_type, short_handler handler);
} // namespace as::wnd

namespace as::wnd
{
wnd_handle& get_window();
} // namespace as::wnd
