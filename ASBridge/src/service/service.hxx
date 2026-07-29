#pragma once

#include "system/string.hxx"

namespace as::liveipc
{
// suppress_auto_register: skip the automatic quickstartregisterwindow/registerlistenerwindow send
// on WINDOW_FOUND (see service.cxx's send_listener_registration). Set by the managed side when it is
// forcibly restarting an already-running bridge and taking registration over itself (see
// AudiosurfHandle's registration watchdog) - a freshly launched bridge with no history yet always
// leaves this false and keeps the normal automatic behavior.
void initialize(as::raw_sys_const_string wnd_title, as::raw_sys_const_string pipe_name, bool suppress_auto_register = false);
void shutdown();
} // namespace as::liveipc
