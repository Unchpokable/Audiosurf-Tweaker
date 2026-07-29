#include "service/service.hxx"
#include "window/native_window.hxx"

#include <thread>

namespace
{
LRESULT process_wm_close(HWND, WPARAM, LPARAM)
{
    ::PostQuitMessage(0);
    return 0;
}

BOOL WINAPI console_ctrl_handler(DWORD ctrl_type)
{
    switch(ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            ::PostMessageW(as::wnd::get_window().native(), WM_CLOSE, 0, 0);
            return TRUE;
        default:
            return FALSE;
    }
}

// Orphan protection: if the tweaker dies without a graceful shutdown (task-killed, crashed), this
// process must not linger forever waiting on a pipe nobody will ever reconnect to. The watcher
// blocks on the parent's process handle and posts WM_CLOSE the moment it becomes signaled.
std::jthread watch_parent_process(DWORD parent_pid)
{
    HANDLE parent = ::OpenProcess(SYNCHRONIZE, FALSE, parent_pid);
    if(parent == nullptr) {
        return {};
    }

    return std::jthread([parent] {
        ::WaitForSingleObject(parent, INFINITE);
        ::CloseHandle(parent);
        ::PostMessageW(as::wnd::get_window().native(), WM_CLOSE, 0, 0);
    });
}
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if(argc < 3) {
        return 1; // usage: ASBridge.exe <window_title> <pipe_name> [parent_pid] [--no-quick-start]
    }

    // Set by the managed side when it is forcibly restarting an already-running bridge instance and
    // taking listener registration over itself - see service.hxx's initialize().
    const bool suppress_auto_register = argc >= 5 && ::_wcsicmp(argv[4], L"--no-quick-start") == 0;

    as::liveipc::initialize(argv[1], argv[2], suppress_auto_register);
    as::wnd::set_handler_for(WM_CLOSE, process_wm_close);

    ::SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    std::jthread parent_watcher;
    if(argc >= 4) {
        parent_watcher = watch_parent_process(static_cast<DWORD>(::_wtoi(argv[3])));
    }

    MSG msg;
    while(::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    as::liveipc::shutdown();

    // The watcher thread never exits on its own while the parent is alive; a graceful shutdown
    // must not block on join. Detach through a local so jthread's destructor doesn't join.
    if(parent_watcher.joinable()) {
        parent_watcher.detach();
    }

    return 0;
}
