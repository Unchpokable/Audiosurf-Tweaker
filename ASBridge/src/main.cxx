#include "service/service.hxx"
#include "window/native_window.hxx"

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
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if(argc < 3) {
        return 1; // usage: ASBridge.exe <window_title> <pipe_name>
    }

    as::liveipc::initialize(argv[1], argv[2]);
    as::wnd::set_handler_for(WM_CLOSE, process_wm_close);

    ::SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    MSG msg;
    while(::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    as::liveipc::shutdown();
    return 0;
}
