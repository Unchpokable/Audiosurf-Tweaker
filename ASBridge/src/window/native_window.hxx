#pragma once

#include "window/native_window.hxx"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#else
#error "This file is only for Windows builds"
#endif

#include "window/wnd_handle.hxx"

#include "system/string.hxx"

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
wnd_handle& get_window();
} // namespace as::wnd
