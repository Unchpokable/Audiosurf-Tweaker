using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace TweakerUI.Core
{
    /// <summary>
    /// Closing Audiosurf and starting it again (Docs/Internal/plugin-offline-mode.md Р-21). Two callers want
    /// exactly this and used to carry their own half-answer: ColorsConfiguratorViewModel shelled out to
    /// "taskkill /f" plus a "cd /d ... && timeout /t 1 && Audiosurf.exe" batch line, and the plugin install
    /// flow (§6.4) needs the same thing done properly.
    ///
    /// "Properly" is the whole reason this exists: a WM_CLOSE first, so the game writes its own options.ini and
    /// Steam records the session, and only then the hard kill - a forced kill is what loses a run in progress
    /// and, for the colours path, the very file that was about to be rewritten.
    /// </summary>
    internal static class GameProcessService
    {
        internal const string ProcessName = "QuestViewer";
        internal const string SteamAppUrl = "steam://rungameid/12900";
        internal const string LauncherFileName = "Audiosurf.exe";

        /// <summary>How long a WM_CLOSE gets before the process is killed instead.</summary>
        private static readonly TimeSpan GracefulCloseTimeout = TimeSpan.FromSeconds(10);

        internal static bool IsRunning => Process.GetProcessesByName(ProcessName).Length > 0;

        /// <summary>The running game, or null. The caller owns the returned process object.</summary>
        internal static Process Find()
        {
            var processes = Process.GetProcessesByName(ProcessName);
            if (processes.Length == 0)
                return null;

            for (var i = 1; i < processes.Length; i++)
                processes[i].Dispose();

            return processes[0];
        }

        /// <summary>
        /// Asks the game to close, then makes it. Returns true once nothing named QuestViewer is left running -
        /// including when there was nothing to close in the first place.
        /// </summary>
        internal static async Task<bool> CloseAsync()
        {
            using var process = Find();
            if (process == null)
                return true;

            try
            {
                if (process.MainWindowHandle != IntPtr.Zero)
                    PostMessage(process.MainWindowHandle, WM_CLOSE, IntPtr.Zero, IntPtr.Zero);

                using (var cts = new CancellationTokenSource(GracefulCloseTimeout))
                {
                    try
                    {
                        await process.WaitForExitAsync(cts.Token);
                        return true;
                    }
                    catch (OperationCanceledException)
                    {
                        // Still up after the grace period - a hung load screen, a modal error box of its own,
                        // or simply a build that ignores WM_CLOSE. Killing it is what the user asked for.
                    }
                }

                Logger.Log("GameProcessService", $"Audiosurf did not close within {GracefulCloseTimeout.TotalSeconds:F0} s - killing it.");
                process.Kill();
                await process.WaitForExitAsync();
                return true;
            }
            catch (Exception ex) when (ex is InvalidOperationException || ex is Win32Exception)
            {
                // Exited between Find() and here, or a handle this process may not touch. The first is success;
                // the second is reported by the wait below coming back false.
                return await WaitUntilGoneAsync(TimeSpan.FromSeconds(2));
            }
        }

        /// <summary>
        /// Waits for the user to close the game themselves (§6.4, "I'll close it myself"). Returns false when the
        /// wait was cancelled and the game is still up.
        /// </summary>
        internal static async Task<bool> WaitForExitAsync(CancellationToken cancellation)
        {
            while (IsRunning)
            {
                if (cancellation.IsCancellationRequested)
                    return false;

                try
                {
                    await Task.Delay(250, cancellation);
                }
                catch (OperationCanceledException)
                {
                    return !IsRunning;
                }
            }

            return true;
        }

        /// <summary>
        /// Starts the game through Steam, so it launches with the overlay, the cloud saves and the playtime it
        /// normally has. <paramref name="gameRootDirectory"/> is the fallback used when no handler is registered
        /// for steam:// at all - which, for a game sold only on Steam, means this copy did not come from there.
        /// </summary>
        internal static bool Start(string gameRootDirectory)
        {
            try
            {
                using (Process.Start(new ProcessStartInfo(SteamAppUrl) { UseShellExecute = true }))
                    return true;
            }
            catch (Exception ex) when (ex is Win32Exception || ex is InvalidOperationException)
            {
                Logger.Log("GameProcessService", $"Could not launch through Steam: {ex.Message}");
            }

            ApplicationNotificationManager.Manager.ShowWarning("Steam not found",
                "Audiosurf could not be started through Steam - there is no handler for steam:// on this machine. " +
                "Audiosurf is a Steam game; if you like it, buy it there. Starting the local copy instead.");

            return StartLocal(gameRootDirectory);
        }

        private static bool StartLocal(string gameRootDirectory)
        {
            if (string.IsNullOrWhiteSpace(gameRootDirectory))
                return false;

            var launcherPath = Path.Combine(gameRootDirectory, LauncherFileName);
            if (!File.Exists(launcherPath))
            {
                ApplicationNotificationManager.Manager.ShowError("Audiosurf not started",
                    $"'{launcherPath}' does not exist - start the game yourself.");
                return false;
            }

            try
            {
                using (Process.Start(new ProcessStartInfo(launcherPath) { UseShellExecute = true, WorkingDirectory = gameRootDirectory }))
                    return true;
            }
            catch (Exception ex) when (ex is Win32Exception || ex is InvalidOperationException)
            {
                ApplicationNotificationManager.Manager.ShowError("Audiosurf not started",
                    $"Could not start '{launcherPath}': {ex.Message}");
                return false;
            }
        }

        private static async Task<bool> WaitUntilGoneAsync(TimeSpan timeout)
        {
            var deadline = DateTime.UtcNow + timeout;
            while (IsRunning)
            {
                if (DateTime.UtcNow >= deadline)
                    return false;

                await Task.Delay(100);
            }

            return true;
        }

        private const uint WM_CLOSE = 0x0010;

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    }
}
