using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace TweakerUI.Core
{
    /// <summary>
    /// Keeps exactly one Audiosurf Tweaker per logon session alive. A named kernel mutex is the
    /// authority - it is acquired atomically and is owned by the kernel, so it disappears on its own
    /// when the holder dies for any reason (crash, task-kill, power loss). A lock file would need its
    /// own liveness/staleness handling for exactly those cases, and a window-title search cannot be
    /// the authority at all: the main window only exists well after startup, so two instances racing
    /// each other would both find nothing and both continue.
    ///
    /// Window lookup is still used, but only as the best-effort "hand the focus over" half; failing
    /// it costs nothing but a notification.
    /// </summary>
    internal static class SingleInstanceGuard
    {
        // Local\ (per logon session), not Global\: the game and the tweaker always live in the same
        // session, so two users on one machine via fast user switching each deserve their own instance.
        private const string MutexName = @"Local\AudiosurfTweaker.SingleInstance";

        private static Mutex _mutex;

        /// <summary>
        /// True when this process is the first instance and now owns the guard. Must be called before
        /// anything else in Main - by design nothing else, in particular no AudiosurfHandle/bridge
        /// service, has started at that point.
        /// </summary>
        internal static bool TryAcquire()
        {
            // Owned by the local until it is known to be ours to keep: the finally below then disposes
            // exactly the handle nobody took, whether that is the normal "somebody else holds it" exit
            // or a throw between the two.
            Mutex candidate = null;
            try
            {
                candidate = new Mutex(true, MutexName, out var createdNew);
                if (!createdNew)
                    return false;

                // Held in a static field on purpose: a collected Mutex would release the guard while
                // the app is still running.
                _mutex = candidate;
                candidate = null;
                return true;
            }
            catch (Exception ex) when (ex is UnauthorizedAccessException || ex is System.IO.IOException)
            {
                // The name exists but this process cannot open it (another session's object leaking
                // through a name collision, restricted token). Treat as "somebody else holds it"
                // rather than starting a second instance behind the guard's back.
                _mutex = null;
                return false;
            }
            finally
            {
                candidate?.Dispose();
            }
        }

        internal static void Release()
        {
            if (_mutex == null)
                return;

            try
            {
                _mutex.ReleaseMutex();
            }
            catch (ApplicationException)
            {
                // Not the owning thread (only possible if Release is ever called off the Main thread) -
                // process exit closes the handle anyway.
            }

            _mutex.Dispose();
            _mutex = null;
        }

        /// <summary>
        /// Best-effort focus handover to the already running instance. Never throws; a false result
        /// just means the caller should tell the user instead.
        /// </summary>
        internal static bool TryActivateRunningInstance()
        {
            var target = FindRunningInstanceWindow();
            if (target == IntPtr.Zero)
                return false;

            GetWindowThreadProcessId(target, out var pid);
            if (pid != 0)
            {
                // This process was just launched by the user, so it currently holds foreground rights;
                // handing them over explicitly is what lets the other process's SetForegroundWindow
                // actually raise the window instead of only flashing its taskbar button.
                AllowSetForegroundWindow(pid);
            }

            if (IsIconic(target))
                ShowWindow(target, SW_RESTORE);

            return SetForegroundWindow(target);
        }

        /// <summary>
        /// Native message box rather than an Avalonia dialog: this runs before AppBuilder has been
        /// started, so there is no toolkit to show anything with. Only reached when the focus handover
        /// above failed - the normal second-launch path stays silent.
        /// </summary>
        internal static void ShowAlreadyRunningMessage()
        {
            MessageBox(IntPtr.Zero,
                "Audiosurf Tweaker is already running, but its window could not be brought to the front.\n\n" +
                "Look for it in the taskbar, or close the running copy and try again.",
                AppShell.MainWindowTitle,
                MB_OK | MB_ICONINFORMATION);
        }

        private static IntPtr FindRunningInstanceWindow()
        {
            var found = IntPtr.Zero;

            EnumWindows((hwnd, _) =>
            {
                if (!IsWindowVisible(hwnd) || !HasTitle(hwnd, AppShell.MainWindowTitle))
                    return true;

                // The title alone is not enough - any unrelated window (an Explorer folder named the
                // same, for one) would match. The owning process's image path settles it.
                if (!IsOwnExecutable(hwnd))
                    return true;

                found = hwnd;
                return false;
            }, IntPtr.Zero);

            return found;
        }

        private static bool HasTitle(IntPtr hwnd, string expected)
        {
            var length = GetWindowTextLength(hwnd);
            if (length != expected.Length)
                return false;

            var builder = new StringBuilder(length + 1);
            if (GetWindowText(hwnd, builder, builder.Capacity) == 0)
                return false;

            return string.Equals(builder.ToString(), expected, StringComparison.Ordinal);
        }

        private static bool IsOwnExecutable(IntPtr hwnd)
        {
            GetWindowThreadProcessId(hwnd, out var pid);
            if (pid == 0)
                return false;

            try
            {
                using var process = Process.GetProcessById((int)pid);
                // Environment.ProcessPath is the apphost .exe on every deploy mode, including
                // self-contained single-file - the same reason AsBridgeConnection.GetBridgePath uses it
                // over AppDomain.CurrentDomain.BaseDirectory.
                return string.Equals(process.MainModule?.FileName, Environment.ProcessPath,
                    StringComparison.OrdinalIgnoreCase);
            }
            catch (Exception ex) when (ex is ArgumentException || ex is InvalidOperationException
                                       || ex is System.ComponentModel.Win32Exception)
            {
                // Process gone between the enumeration and this check, or its module list is not
                // readable - either way this is not the window we are looking for.
                return false;
            }
        }

        private const int SW_RESTORE = 9;
        private const uint MB_OK = 0x0;
        private const uint MB_ICONINFORMATION = 0x40;

        private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lparam);

        [DllImport("user32.dll")]
        private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lparam);

        [DllImport("user32.dll")]
        private static extern bool IsWindowVisible(IntPtr hwnd);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetWindowTextW")]
        private static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maxCount);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetWindowTextLengthW")]
        private static extern int GetWindowTextLength(IntPtr hwnd);

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

        [DllImport("user32.dll")]
        private static extern bool IsIconic(IntPtr hwnd);

        [DllImport("user32.dll")]
        private static extern bool ShowWindow(IntPtr hwnd, int command);

        [DllImport("user32.dll")]
        private static extern bool SetForegroundWindow(IntPtr hwnd);

        [DllImport("user32.dll")]
        private static extern bool AllowSetForegroundWindow(uint processId);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "MessageBoxW")]
        private static extern int MessageBox(IntPtr hwnd, string text, string caption, uint type);
    }
}
