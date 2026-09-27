using System;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace TweakerUI.Core.Utils
{
    public static class Utils
    {
        public static void Cmd(string command)
        {
            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = $"/c {command}",
                    WindowStyle = ProcessWindowStyle.Hidden,
                });
            }
            catch (Exception ex)
            {
                // Fire-and-forget by design - the caller has nothing to react with - but a silent
                // failure here was indistinguishable from the command running and doing nothing.
                Logger.Log("Utils.Cmd", $"'{command}' could not be started: {ex}");
            }
        }

        /// <summary>
        /// Opens a URL in whatever the user's default browser is. `UseShellExecute` is the whole trick: without
        /// it .NET tries to execute the URL as a program and fails.
        ///
        /// Fire-and-forget like <see cref="Cmd"/> — a caller that has just told the user "go to this page" has
        /// nothing useful to do if the shell refuses, beyond the log line below.
        /// </summary>
        public static void OpenUrl(string url)
        {
            try
            {
                using (Process.Start(new ProcessStartInfo(url) { UseShellExecute = true }))
                {
                }
            }
            catch (Exception ex)
            {
                Logger.Log("Utils.OpenUrl", $"'{url}' could not be opened: {ex}");
            }
        }

        public static void HardClear(string path)
        {
            // Absolute and fast annihilation of any content in specified folder
            if (!Directory.Exists(path))
                return;

            try
            {
                Directory.GetFiles(path).AsParallel().ForAll(file =>
                {
                    try
                    {
                        // Was clearing the attribute on the *folder* rather than on the file being
                        // deleted, which is why read-only leftovers survived a HardClear.
                        File.SetAttributes(file, File.GetAttributes(file) & ~FileAttributes.ReadOnly);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log("Utils.HardClear", $"could not clear the read-only attribute on '{file}': {ex.Message}");
                    }

                    try
                    {
                        File.Delete(file);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log("Utils.HardClear", $"could not delete '{file}': {ex.Message}");
                    }
                });

                Directory.GetDirectories(path).AsParallel().ForAll(directory =>
                {
                    try
                    {
                        Directory.Delete(directory, true);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log("Utils.HardClear", $"could not delete '{directory}': {ex.Message}");
                    }
                });
            }
            catch (Exception ex)
            {
                // Whatever the enumeration itself threw (the folder going away mid-sweep, an entry
                // that cannot be read), wrapped by AsParallel().ForAll into an AggregateException.
                Logger.Log("Utils.HardClear", $"sweep of '{path}' did not complete: {ex}");
            }
        }
    }
}
