#pragma once

#ifndef HT_FILE_HXX
#define HT_FILE_HXX

#if defined(_WIN32)

#define HT_FILE_ON_WINDOWS

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "string.hxx"

namespace as
{
using file_handle = HANDLE;
using system_path = as::raw_sys_const_string;
using flags_bitmask = DWORD;
} // namespace as

#else
#error "High Tier core does not support non-windows platform yet!"
#endif

#endif
