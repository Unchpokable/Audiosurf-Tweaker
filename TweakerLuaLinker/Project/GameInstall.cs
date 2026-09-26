using System;
using System.IO;

namespace TweakerLuaLinker.Project
{
    /// <summary>
    /// `twllink --install`: the built script and its map, copied into the game's
    /// engine\TweakerStuff\Scripts\ (lua-engine-fix-roadmap.md §9.7). Where the game is, is the machine's
    /// business and never the project's (§9.4): --game on the command line, or AUDIOSURF_DIR - the same
    /// variable Tools/CgrPy reads, pointing at the folder that holds engine\.
    /// </summary>
    internal static class GameInstall
    {
        public const string EnvironmentVariable = "AUDIOSURF_DIR";

        /// <summary>The Scripts folder to install into, or null with the reason.</summary>
        public static string? ScriptsFolder(string? game, out string? error)
        {
            error = null;
            game ??= Environment.GetEnvironmentVariable(EnvironmentVariable);
            if (string.IsNullOrWhiteSpace(game))
            {
                error = $"no game to install into - pass --game <folder> or set {EnvironmentVariable} to the folder that holds engine\\";
                return null;
            }

            var engine = File.Exists(Path.Combine(game, "QuestViewer.exe")) ? game
                : File.Exists(Path.Combine(game, "engine", "QuestViewer.exe")) ? Path.Combine(game, "engine")
                : null;
            if (engine == null)
            {
                error = $"{game} is not an Audiosurf install - neither it nor its engine\\ folder has QuestViewer.exe";
                return null;
            }

            var stuff = Path.Combine(engine, "TweakerStuff");
            if (!Directory.Exists(stuff))
            {
                error = $"the Tweaker's plugin is not installed in {engine} (there is no TweakerStuff\\ folder) - install it from the Tweaker first";
                return null;
            }

            return Path.Combine(stuff, "Scripts");
        }

        /// <summary>Copies script and map; returns the installed script's path. IO failures throw.</summary>
        public static string Install(string scriptsFolder, string bundlePath, string mapPath)
        {
            Directory.CreateDirectory(scriptsFolder);
            var target = Path.Combine(scriptsFolder, Path.GetFileName(bundlePath));
            CopyIfChanged(bundlePath, target);
            CopyIfChanged(mapPath, target + ".map");
            return target;
        }

        // An unchanged file keeps its timestamp - nothing watching the Scripts folder should see a change
        // that is not one.
        private static void CopyIfChanged(string from, string to)
        {
            var bytes = File.ReadAllBytes(from);
            if (File.Exists(to) && File.ReadAllBytes(to).AsSpan().SequenceEqual(bytes))
                return;
            File.WriteAllBytes(to, bytes);
        }
    }
}
