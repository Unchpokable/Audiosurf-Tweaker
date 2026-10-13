using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

using TweakerCore.PluginInstall;

namespace TweakerCore.Scripting
{
    /// <summary>Whose a script in the game's Scripts folder is, going by the deployment manifest.</summary>
    public enum ScriptOrigin
    {
        /// <summary>Not written by Tweaker: the user's own, or installed by twllink.</summary>
        User,

        /// <summary>Written by Tweaker and unchanged since.</summary>
        Shipped,

        /// <summary>Written by Tweaker and changed since - the next plugin update moves this copy to ".old".</summary>
        Modified
    }

    /// <summary>One script as the Scripts tab of TweakerUI lists it.</summary>
    public sealed class GameScript
    {
        /// <summary>"puzzlepro_hud.lua" - the script's identity: scripts.cfg keys it by this, and so does the plugin.</summary>
        public string FileName { get; init; }

        /// <summary>Full path; for a <see cref="IsMissing"/> script, where it would be.</summary>
        public string Path { get; init; }

        public ScriptHeader Header { get; init; }

        public bool Enabled { get; init; }

        public ScriptOrigin Origin { get; init; }

        /// <summary>A .lua.map lies beside it: twllink built it, and its errors will point into the project's files.</summary>
        public bool HasMap { get; init; }

        /// <summary>
        /// The bundle ships a script by this name, and what is on disk is not what the bundle has - restoring
        /// would change it. Also true for a <see cref="IsMissing"/> one.
        /// </summary>
        public bool Restorable { get; init; }

        /// <summary>The bundle ships it and it is not on disk - deleted by the user, or never installed.</summary>
        public bool IsMissing { get; init; }

        /// <summary>"foo.lua.old", "foo.lua.old.2", ... - the user's copies an update moved aside, oldest first.</summary>
        public IReadOnlyList<string> Backups { get; init; }
    }

    /// <summary>
    /// What is in the game's engine\TweakerStuff\Scripts\ and what it is - read from the files, the deployment
    /// manifest, scripts.cfg and the plugin bundle, never from the running plugin (what the plugin says is laid
    /// over it by the Scripts tab - Docs/Internal/script-manager.md §5, §7).
    ///
    /// The folder is read the way the plugin reads it: flat, and only files whose extension is exactly ".lua" -
    /// the plugin's comparison is case-sensitive, so "X.LUA" is not a script to it, and must not be one here.
    /// </summary>
    public static class GameScripts
    {
        public const string ScriptExtension = ".lua";
        public const string MapSuffix = ".map";
        private const string BackupSuffix = ".old";

        /// <summary>
        /// Every script on disk, sorted by file name the way the plugin sorts them, followed by the bundle's
        /// scripts that are not on disk. <paramref name="payloadDirectory"/> may be null or missing - then
        /// nothing is restorable and nothing is missing.
        /// </summary>
        public static IReadOnlyList<GameScript> Scan(string engineDirectory, string payloadDirectory)
        {
            var result = new List<GameScript>();
            if (!PluginInstallation.IsValidEngineFolder(engineDirectory))
                return result;

            var folder = PluginInstallation.ScriptsPath(engineDirectory);
            var files = Directory.Exists(folder) ? Directory.EnumerateFiles(folder).Select(Path.GetFileName).ToList() : new List<string>();
            var present = new HashSet<string>(files, StringComparer.OrdinalIgnoreCase);

            var payloadFolder = string.IsNullOrWhiteSpace(payloadDirectory)
                ? null
                : Path.Combine(payloadDirectory, PluginInstallation.StuffFolderName, PluginInstallation.ScriptsFolderName);
            var shipped = payloadFolder != null && Directory.Exists(payloadFolder)
                ? Directory.EnumerateFiles(payloadFolder).Where(IsScript).ToList()
                : new List<string>();

            var manifest = PluginDeployManifest.Load(engineDirectory);
            var config = ScriptsConfig.Load(engineDirectory);

            foreach (var file in files.Where(IsScript).OrderBy(name => name, StringComparer.Ordinal))
            {
                var path = Path.Combine(folder, file);
                var hash = TryHash(path);
                var payloadFile = shipped.FirstOrDefault(p => string.Equals(Path.GetFileName(p), file, StringComparison.OrdinalIgnoreCase));

                result.Add(new GameScript
                {
                    FileName = file,
                    Path = path,
                    Header = ScriptHeader.Read(path),
                    Enabled = config.IsEnabled(file),
                    Origin = OriginOf(manifest, file, hash),
                    HasMap = present.Contains(file + MapSuffix),
                    Restorable = payloadFile != null && !PluginInstallation.HashesEqual(hash, TryHash(payloadFile)),
                    Backups = BackupsOf(file, files)
                });
            }

            foreach (var payloadFile in shipped.OrderBy(p => p, StringComparer.Ordinal))
            {
                var file = Path.GetFileName(payloadFile);
                if (present.Contains(file))
                    continue;

                result.Add(new GameScript
                {
                    FileName = file,
                    Path = Path.Combine(folder, file),
                    Header = ScriptHeader.Read(payloadFile),
                    Enabled = config.IsEnabled(file),
                    Origin = ScriptOrigin.Shipped,
                    Restorable = true,
                    IsMissing = true,
                    Backups = BackupsOf(file, files)
                });
            }

            return result;
        }

        /// <summary>
        /// Whether the plugin bundle ships a script under this file name - what a project whose id collides with
        /// a bundled script is warned about (Docs/Internal/script-manager.md §8.6). The manifest
        /// counts too: a script shipped by an earlier version is still recorded as Tweaker's in this game.
        /// </summary>
        public static bool IsShipped(string engineDirectory, string payloadDirectory, string scriptFileName)
        {
            if (!string.IsNullOrWhiteSpace(payloadDirectory)
                && File.Exists(Path.Combine(payloadDirectory, PluginInstallation.StuffFolderName, PluginInstallation.ScriptsFolderName, scriptFileName)))
            {
                return true;
            }
            return PluginInstallation.IsValidEngineFolder(engineDirectory)
                   && PluginDeployManifest.Load(engineDirectory).TryGetHash(PluginInstallation.ScriptsRelativeFolder + scriptFileName, out _);
        }

        private static bool IsScript(string path)
        {
            return string.Equals(Path.GetExtension(path), ScriptExtension, StringComparison.Ordinal);
        }

        private static ScriptOrigin OriginOf(PluginDeployManifest manifest, string file, string hash)
        {
            if (!manifest.TryGetHash(PluginInstallation.ScriptsRelativeFolder + file, out var recorded))
                return ScriptOrigin.User;
            return PluginInstallation.HashesEqual(recorded, hash) ? ScriptOrigin.Shipped : ScriptOrigin.Modified;
        }

        /// <summary>"foo.lua.old" first, then ".old.2", ".old.3" ... by number - the order PluginInstallation makes them in.</summary>
        private static IReadOnlyList<string> BackupsOf(string file, List<string> files)
        {
            var plain = file + BackupSuffix;
            var numbered = plain + ".";
            return files
                .Select(name => (Name: name, Order: string.Equals(name, plain, StringComparison.OrdinalIgnoreCase) ? 1 : NumberAfter(name, numbered)))
                .Where(backup => backup.Order > 0)
                .OrderBy(backup => backup.Order)
                .Select(backup => backup.Name)
                .ToList();
        }

        private static int NumberAfter(string name, string prefix)
        {
            return name.StartsWith(prefix, StringComparison.OrdinalIgnoreCase) && int.TryParse(name.AsSpan(prefix.Length), out var n) && n >= 2 ? n : 0;
        }

        private static string TryHash(string path)
        {
            try
            {
                return PluginInstallation.HashFile(path);
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return null;
            }
        }
    }
}
