using System;
using System.IO;
using System.Text;
using System.Threading;

namespace TweakerLuaLinker
{
    /// <summary>
    /// twllink.exe - the Lua project linker for TweakerPlugin scripts (lua-engine-fix-roadmap.md §9, Ф5).
    /// Everything lives in <see cref="Cli"/>; this only turns a crash into exit code 3, so a caller can tell
    /// "the project has errors" from "the tool is broken" (§9.7), and Ctrl+C into the end of --watch
    /// rather than a killed process.
    /// </summary>
    internal static class Program
    {
        private static int Main(string[] args)
        {
            Console.OutputEncoding = new UTF8Encoding(false);
            using var cancel = new CancellationTokenSource();
            Console.CancelKeyPress += (_, e) =>
            {
                e.Cancel = true;
                cancel.Cancel();
            };

            try
            {
                return Cli.Run(args, Console.Out, Console.Error, Directory.GetCurrentDirectory(), cancel.Token);
            }
            catch (Exception e)
            {
                Console.Error.WriteLine($"twllink: internal error - {e.GetType().Name}: {e.Message}");
                Console.Error.WriteLine(e.StackTrace);
                return Cli.ExitInternal;
            }
        }
    }
}
