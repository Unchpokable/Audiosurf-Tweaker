#pragma once

#ifndef HT_STRING_HXX
#define HT_STRING_HXX

#if defined(_WIN32)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>

namespace as
{
using sys_string = std::wstring;
using sys_string_view = std::wstring_view;
using raw_sys_string = LPWSTR;
using raw_sys_const_string = LPCWSTR;
} // namespace as

#else
#error "High Tier core does not support non-windows platform yet!"
#endif

#endif
