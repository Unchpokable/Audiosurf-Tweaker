#pragma once

#include "system/string.hxx"

namespace as::liveipc
{
void initialize(as::raw_sys_const_string wnd_title, as::raw_sys_const_string pipe_name);
void shutdown();
} // namespace as::liveipc
