#include "pch.hxx"

#include "plugin/diagnostics.hxx"

#if TW_DIAGNOSTICS_ENABLED

namespace
{
// Only true when AllocConsole() actually created one for us - a game launched from a console (or
// already owning one) must keep it, and must not have it freed out from under it by shutdown().
bool g_owns_console = false;
bool g_sinks_registered = false;

// Turns on ANSI escape handling so uulog::builtin::stdout_ansi's color codes render as colors
// rather than as literal "\033[1;32m" garbage. Available since Windows 10 1511; on anything older
// the call fails and the sink degrades to noisy-but-readable text, which is why the result is
// ignored.
void enable_virtual_terminal(HANDLE console_out) noexcept
{
    DWORD mode = 0;
    if(::GetConsoleMode(console_out, &mode) == FALSE) {
        return;
    }

    ::SetConsoleMode(console_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

// Closing the console window terminates every process attached to it - i.e. the game. Since this
// console is a debugging aid bolted onto someone else's process, take the close box away rather
// than hand the user a one-click way to kill their game mid-session.
void disable_console_close_button() noexcept
{
    const HWND console_window = ::GetConsoleWindow();
    if(console_window == nullptr) {
        return;
    }

    HMENU system_menu = ::GetSystemMenu(console_window, FALSE);
    if(system_menu != nullptr) {
        ::DeleteMenu(system_menu, SC_CLOSE, MF_BYCOMMAND);
    }
}

bool attach_console() noexcept
{
    // A console the process already owns (started from cmd, or a previous initialize()) is reused
    // as-is: AllocConsole would just fail, and freeing it later would not be ours to do.
    if(::GetConsoleWindow() != nullptr) {
        return false;
    }

    if(::AllocConsole() == FALSE) {
        return false;
    }

    FILE* stream = nullptr;
    ::freopen_s(&stream, "CONOUT$", "w", stdout);
    ::freopen_s(&stream, "CONOUT$", "w", stderr);

    ::SetConsoleTitleW(L"TweakerPlugin (debug)");
    ::SetConsoleOutputCP(CP_UTF8);
    enable_virtual_terminal(::GetStdHandle(STD_OUTPUT_HANDLE));
    disable_console_close_button();

    return true;
}
} // namespace

namespace tw::plugin::diagnostics
{
void initialize() noexcept
{
    if(g_sinks_registered) {
        return;
    }

    g_owns_console = attach_console();

    uulog::add_sink(&uulog::builtin::stdout_ansi);
    uulog::add_sink(&uulog::builtin::win32_debugout);
    g_sinks_registered = true;

    TW_LOG_INFO("diagnostics online (console={}, pid={})", g_owns_console ? "allocated" : "inherited", ::GetCurrentProcessId());
}

void shutdown() noexcept
{
    if(!g_sinks_registered) {
        return;
    }

    uulog::remove_sink(&uulog::builtin::stdout_ansi);
    uulog::remove_sink(&uulog::builtin::win32_debugout);
    g_sinks_registered = false;

    if(g_owns_console) {
        ::FreeConsole();
        g_owns_console = false;
    }
}
} // namespace tw::plugin::diagnostics

#else

namespace tw::plugin::diagnostics
{
void initialize() noexcept
{
}

void shutdown() noexcept
{
}
} // namespace tw::plugin::diagnostics

#endif
