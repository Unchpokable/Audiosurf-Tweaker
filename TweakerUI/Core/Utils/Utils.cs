using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace TweakerUI.Core.Utils
{
    public static class Utils
    {
        /// <summary>
        /// Disposes the given objects and, a second later, forces a full collection.
        ///
        /// Forcing the GC by hand is normally a smell; here it is deliberate. Everything handed to
        /// this method owns SkiaSharp bitmaps, whose pixel buffers are unmanaged - the GC sees a
        /// handful of tiny wrappers and no reason to hurry, so freed skin textures sat on hundreds of
        /// megabytes for minutes at a time. Delayed and off the calling thread so a collection never
        /// lands inside the operation that triggered it. Fire-and-forget by design - there is nothing
        /// for a caller to await or react to.
        /// </summary>
        public static void DisposeAndClear(params IDisposable[] disposables)
        {
            foreach (var disposable in disposables)
                disposable?.Dispose();

            _ = Task.Delay(1000).ContinueWith(_ =>
            {
                GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced, false, true);
                GC.WaitForPendingFinalizers();
                GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced, false, true);
            });
        }

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
