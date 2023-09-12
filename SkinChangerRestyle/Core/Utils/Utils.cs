using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SkinChangerRestyle.Core.Utils
{
    public static class Utils
    {
        public static async void DisposeAndClear(params IDisposable[] disposable)
        {
            foreach (var d in disposable)
                d?.Dispose();

            await Task.Run(async () =>
            {
                await Task.Delay(1000);
                // Ye, i know that manual calling GC.Collct() is a very bad practice, but idk why, in this certain case GC works as shit bag and lefts OVER NINE THOUSANDS unused memory for an undefined long while
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
            catch
            {
                // ignored
            }
        }

        public static void HardClear(string path)
        {
            // Absolute and fast annihilation of any content in specified folder
            if (Directory.Exists(path))
            {
                try
                {
                    Directory.GetFiles(path).AsParallel().ForAll(x =>
                    {
                        try { File.SetAttributes(path, File.GetAttributes(path) & ~FileAttributes.ReadOnly); } catch { }
                        try { File.Delete(x); } catch { }
                    });
                    Directory.GetDirectories(path).AsParallel().ForAll(x =>
                    {
                        try { Directory.Delete(x, true); } catch { };
                    });
                }
                catch { }
            }
        }
    }
}
