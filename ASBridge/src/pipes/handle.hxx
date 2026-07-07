#pragma once

#ifndef HT_CORE_SYSTEM_HXX
#define HT_CORE_SYSTEM_HXX

#if defined(_WIN32)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace as
{
using proc_handle = HANDLE;
using pid_t = DWORD;
} // namespace as

namespace as
{
inline as::proc_handle invalid_handle = INVALID_HANDLE_VALUE;
} // namespace as

namespace as
{
inline constexpr auto wait_infinite = INFINITE;
} // namespace as

#else
#error "High Tier core does not support non-windows platform yet!"
#endif

#endif
