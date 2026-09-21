using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;

namespace TweakerCore.PluginInstall
{
    /// <summary>
    /// Putting TweakerPlugin into the game folder, keeping it up to date, and taking it out again
    /// (Docs/Internal/plugin-offline-mode.md §6.3). File logic only: no UI, no dialogs and no process handling -
    /// whether the game is running is the caller's question to answer (§6.4), because answering it means asking
    /// the user.
    ///
    /// The layout it maintains, anchored at the game's engine folder:
    ///
    ///     engine\channels\TweakerPlugin.dll     the plugin, loaded by the engine itself
    ///     engine\TweakerStuff\Scripts\*.lua     bundled scripts
    ///     engine\TweakerStuff\.deploy.json      what of the above Tweaker wrote, and with what content
    ///
    /// The rule the whole module exists to keep: a file the user changed is never overwritten without a copy of
    /// it surviving, and anything under TweakerStuff that Tweaker did not write is not Tweaker's to touch.
    /// </summary>
    public static class PluginInstallation
    {
        public const string PluginFileName = "TweakerPlugin.dll";
        public const string ChannelsFolderName = "channels";
        public const string StuffFolderName = "TweakerStuff";
        public const string GameExecutableName = "QuestViewer.exe";

        /// <summary>Manifest key of the plugin itself.</summary>
        public const string PluginRelativePath = ChannelsFolderName + "/" + PluginFileName;

        /// <summary>Appended to a user's own copy before a shipped file replaces it; ".old.2", ".old.3" on collision.</summary>
        private const string BackupSuffix = ".old";

        /// <summary>
        /// A folder is the game's engine folder when QuestViewer.exe sits in it next to channels\. Both matter:
        /// the exe says this is Audiosurf's engine folder, channels\ is where the plugin has to end up.
        /// </summary>
        public static bool IsValidEngineFolder(string engineDirectory)
        {
            if (string.IsNullOrWhiteSpace(engineDirectory))
                return false;

            return File.Exists(Path.Combine(engineDirectory, GameExecutableName))
                   && Directory.Exists(Path.Combine(engineDirectory, ChannelsFolderName));
        }

        /// <summary>
        /// The engine folder behind the one path the user actually configures - the textures folder
        /// (engine\textures). Null when that setting is empty or does not point where it should.
        /// </summary>
        public static string EngineFolderFromTexturesPath(string texturesPath)
        {
            if (string.IsNullOrWhiteSpace(texturesPath))
                return null;

            var trimmed = texturesPath.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var parent = Directory.GetParent(trimmed);
            if (parent == null)
                return null;

            return IsValidEngineFolder(parent.FullName) ? parent.FullName : null;
        }

        public static string PluginPath(string engineDirectory)
        {
            return Path.Combine(engineDirectory, ChannelsFolderName, PluginFileName);
        }

        /// <summary>Whether the plugin is in the game folder - the truth behind the host's "plugin installed" setting (Р-6).</summary>
        public static bool IsInstalled(string engineDirectory)
        {
            return !string.IsNullOrWhiteSpace(engineDirectory) && File.Exists(PluginPath(engineDirectory));
        }

        /// <summary>Version of the plugin sitting in the game folder, or null when it is not there at all.</summary>
        public static string InstalledVersion(string engineDirectory)
        {
            return IsInstalled(engineDirectory) ? TryReadVersion(PluginPath(engineDirectory)) : null;
        }

        /// <summary>Version of the plugin this bundle would install, or null when the bundle carries none.</summary>
        public static string PayloadVersion(string payloadDirectory)
        {
            var path = PayloadPluginPath(payloadDirectory);
            return path != null && File.Exists(path) ? TryReadVersion(path) : null;
        }

        /// <summary>
        /// Whether a folder is a usable bundle: it has the plugin where the target layout puts it (§6.1). The
        /// single thing worth checking before touching anything - everything else a bundle carries is optional.
        /// </summary>
        public static bool HasPlugin(string payloadDirectory)
        {
            var path = PayloadPluginPath(payloadDirectory);
            return path != null && File.Exists(path);
        }

        private static string PayloadPluginPath(string payloadDirectory)
        {
            return string.IsNullOrWhiteSpace(payloadDirectory)
                ? null
                : Path.Combine(payloadDirectory, ToFileSystemPath(PluginRelativePath));
        }

        /// <summary>
        /// Whether <see cref="Install"/> would touch a single file on disk - the question the host has to answer
        /// before it decides whether an update is worth asking the user to close the game for (§6.4). Reads only.
        ///
        /// Manifest-only outcomes deliberately do not count: a file the user edited and the bundle stopped
        /// shipping is merely forgotten (<see cref="PluginInstallReport.Released"/>), and re-recording a hash for
        /// a file that already matches the bundle changes nothing the game can see.
        /// </summary>
        public static bool NeedsUpdate(string engineDirectory, string payloadDirectory)
        {
            if (!IsValidEngineFolder(engineDirectory) || string.IsNullOrWhiteSpace(payloadDirectory) || !Directory.Exists(payloadDirectory))
                return false;

            // Nothing Install would refuse outright can be pending, or startup would keep proposing an update
            // that cannot happen.
            if (!HasPlugin(payloadDirectory))
                return false;

            var manifest = PluginDeployManifest.Load(engineDirectory);
            var shipped = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            foreach (var payloadFile in Directory.EnumerateFiles(payloadDirectory, "*", SearchOption.AllDirectories))
            {
                var relativePath = ToManifestPath(Path.GetRelativePath(payloadDirectory, payloadFile));
                shipped.Add(relativePath);

                try
                {
                    var targetPath = Path.Combine(engineDirectory, ToFileSystemPath(relativePath));
                    var decision = Decide(targetPath, payloadFile, relativePath, HashFile(payloadFile), manifest);

                    if (decision != FileDecision.None && decision != FileDecision.SkipDeleted)
                        return true;
                }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                {
                    // Unreadable right now means Install would log an error for this file, not that it would
                    // change it - and an error is not a reason to ask the user to close their game.
                }
            }

            foreach (var relativePath in manifest.Files.Keys)
            {
                if (shipped.Contains(relativePath))
                    continue;

                try
                {
                    var targetPath = Path.Combine(engineDirectory, ToFileSystemPath(relativePath));
                    if (File.Exists(targetPath)
                        && manifest.TryGetHash(relativePath, out var recordedHash)
                        && HashesEqual(recordedHash, HashFile(targetPath)))
                    {
                        return true;
                    }
                }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                {
                }
            }

            return false;
        }

        /// <summary>
        /// Copies the bundle - whose layout mirrors the target: channels\, TweakerStuff\ (§6.1) - into the game
        /// folder, resolving every file by the table in §6.3, and rewrites the manifest.
        ///
        /// Never throws for a file it could not handle: the failure goes into the report and the rest of the
        /// bundle is still installed. A partly installed plugin is worth more than an exception halfway through
        /// the tree.
        /// </summary>
        public static PluginInstallReport Install(string engineDirectory, string payloadDirectory)
        {
            var report = new PluginInstallReport();

            if (!IsValidEngineFolder(engineDirectory))
            {
                report.Errors.Add(new PluginInstallError(null, "'" + engineDirectory + "' is not the game's engine folder."));
                return report;
            }

            if (string.IsNullOrWhiteSpace(payloadDirectory) || !Directory.Exists(payloadDirectory))
            {
                report.Errors.Add(new PluginInstallError(null, "Plugin payload folder '" + payloadDirectory + "' does not exist."));
                return report;
            }

            // A bundle without the plugin in it is not a bundle, and installing it anyway is worse than refusing:
            // every file it does carry gets recorded in the manifest as Tweaker's, which is a licence to delete
            // those files on some later update. A folder assembled by hand with the DLL loose at the top is the
            // shape this actually catches - the DLL then reads as an ordinary payload file bound for engine\.
            if (!HasPlugin(payloadDirectory))
            {
                report.Errors.Add(new PluginInstallError(PluginRelativePath,
                    "'" + payloadDirectory + "' has no " + PluginRelativePath + " in it. A bundle mirrors the game's own "
                    + "layout, so the DLL belongs in a " + ChannelsFolderName + "\\ subfolder of it. Nothing was installed."));
                return report;
            }

            var manifest = PluginDeployManifest.Load(engineDirectory);
            var shipped = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            foreach (var payloadFile in Directory.EnumerateFiles(payloadDirectory, "*", SearchOption.AllDirectories))
            {
                var relativePath = ToManifestPath(Path.GetRelativePath(payloadDirectory, payloadFile));
                shipped.Add(relativePath);

                try
                {
                    InstallOne(engineDirectory, payloadFile, relativePath, manifest, report);
                }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                {
                    report.Errors.Add(new PluginInstallError(relativePath, exception.Message));
                }
            }

            RemoveDropped(engineDirectory, shipped, manifest, report);

            var pluginPath = PluginPath(engineDirectory);
            if (File.Exists(pluginPath))
                manifest.PluginVersion = TryReadVersion(pluginPath) ?? manifest.PluginVersion;

            SaveManifest(engineDirectory, manifest, report);

            return report;
        }

        /// <summary>
        /// Takes the plugin out of the game: the DLL and its manifest entry, nothing else. TweakerStuff stays
        /// exactly as it is - skyboxes, scripts and configs are the user's data, and removing the plugin is not a
        /// request to delete them (§6.3).
        /// </summary>
        public static PluginInstallReport Uninstall(string engineDirectory)
        {
            var report = new PluginInstallReport();

            if (!IsValidEngineFolder(engineDirectory))
            {
                report.Errors.Add(new PluginInstallError(null, "'" + engineDirectory + "' is not the game's engine folder."));
                return report;
            }

            var pluginPath = PluginPath(engineDirectory);

            try
            {
                if (File.Exists(pluginPath))
                {
                    File.Delete(pluginPath);
                    report.Removed.Add(PluginRelativePath);
                }
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                report.Errors.Add(new PluginInstallError(PluginRelativePath, exception.Message));
                return report;
            }

            var manifest = PluginDeployManifest.Load(engineDirectory);

            // No manifest means nothing to rewrite - and writing one here would recreate TweakerStuff\ for a
            // plugin that is on its way out.
            if (!manifest.Exists)
                return report;

            manifest.Remove(PluginRelativePath);
            manifest.PluginVersion = null;

            SaveManifest(engineDirectory, manifest, report);

            return report;
        }

        /// <summary>What §6.3's table says should happen to one file. Decided before anything is written, so that
        /// <see cref="NeedsUpdate"/> can ask the same question without answering it differently.</summary>
        private enum FileDecision
        {
            /// <summary>On disk already exactly as the bundle has it - only the manifest is brought up to date.</summary>
            None,
            Add,
            /// <summary>Shipped once, deleted by the user since - putting it back would make deleting it impossible.</summary>
            SkipDeleted,
            Overwrite,
            BackupAndOverwrite
        }

        private static FileDecision Decide(
            string targetPath,
            string payloadFile,
            string relativePath,
            string payloadHash,
            PluginDeployManifest manifest)
        {
            if (!File.Exists(targetPath))
                return manifest.TryGetHash(relativePath, out _) ? FileDecision.SkipDeleted : FileDecision.Add;

            // The DLL is never the user's, so it is never backed up - it is replaced whenever it differs. Version
            // first because that is the cheap answer and the meaningful one; the hash decides when two builds
            // carry the same version, which is exactly what a rebuilt development copy does.
            if (IsPlugin(relativePath))
            {
                var payloadVersion = TryReadVersion(payloadFile);
                var installedVersion = TryReadVersion(targetPath);

                var versionsMatch = payloadVersion == null
                                    || installedVersion == null
                                    || string.Equals(payloadVersion, installedVersion, StringComparison.OrdinalIgnoreCase);

                return versionsMatch && HashesEqual(HashFile(targetPath), payloadHash)
                    ? FileDecision.None
                    : FileDecision.Overwrite;
            }

            var targetHash = HashFile(targetPath);

            if (HashesEqual(targetHash, payloadHash))
                return FileDecision.None;

            // Untouched since Tweaker wrote it - overwrite without a word.
            if (manifest.TryGetHash(relativePath, out var recordedHash) && HashesEqual(recordedHash, targetHash))
                return FileDecision.Overwrite;

            // Either the user edited a shipped file, or they had a file of their own under the same name - the
            // same thing as far as this module is concerned, and the same answer: their version survives.
            return FileDecision.BackupAndOverwrite;
        }

        private static void InstallOne(
            string engineDirectory,
            string payloadFile,
            string relativePath,
            PluginDeployManifest manifest,
            PluginInstallReport report)
        {
            var targetPath = Path.Combine(engineDirectory, ToFileSystemPath(relativePath));
            var payloadHash = HashFile(payloadFile);

            switch (Decide(targetPath, payloadFile, relativePath, payloadHash, manifest))
            {
                case FileDecision.SkipDeleted:
                    report.SkippedDeleted.Add(relativePath);
                    return;

                case FileDecision.Add:
                    Copy(payloadFile, targetPath);
                    manifest.Set(relativePath, payloadHash);
                    report.Added.Add(relativePath);
                    return;

                case FileDecision.Overwrite:
                    Copy(payloadFile, targetPath);
                    manifest.Set(relativePath, payloadHash);
                    report.Updated.Add(relativePath);
                    return;

                case FileDecision.BackupAndOverwrite:
                    var backupPath = MakeBackupPath(targetPath);
                    File.Move(targetPath, backupPath);
                    Copy(payloadFile, targetPath);
                    manifest.Set(relativePath, payloadHash);
                    report.UpdatedWithBackup.Add(new PluginFileBackup(relativePath, Path.GetFileName(backupPath)));
                    return;

                default:
                    // Nothing to copy; the manifest is recorded so the file counts as ours from here on.
                    manifest.Set(relativePath, payloadHash);
                    return;
            }
        }

        /// <summary>
        /// Files the manifest remembers but the bundle no longer ships. Untouched ones go with the bundle they
        /// came from; edited ones stay and stop being Tweaker's business.
        /// </summary>
        private static void RemoveDropped(
            string engineDirectory,
            HashSet<string> shipped,
            PluginDeployManifest manifest,
            PluginInstallReport report)
        {
            foreach (var relativePath in manifest.Files.Keys.ToList())
            {
                if (shipped.Contains(relativePath))
                    continue;

                var targetPath = Path.Combine(engineDirectory, ToFileSystemPath(relativePath));

                try
                {
                    if (!File.Exists(targetPath))
                    {
                        manifest.Remove(relativePath);
                        continue;
                    }

                    if (manifest.TryGetHash(relativePath, out var recordedHash) && HashesEqual(recordedHash, HashFile(targetPath)))
                    {
                        File.Delete(targetPath);
                        manifest.Remove(relativePath);
                        report.Removed.Add(relativePath);
                        continue;
                    }

                    manifest.Remove(relativePath);
                    report.Released.Add(relativePath);
                }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                {
                    report.Errors.Add(new PluginInstallError(relativePath, exception.Message));
                }
            }
        }

        private static void SaveManifest(string engineDirectory, PluginDeployManifest manifest, PluginInstallReport report)
        {
            try
            {
                manifest.Save(engineDirectory);
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                report.Errors.Add(new PluginInstallError(PluginDeployManifest.FileName, exception.Message));
            }
        }

        /// <summary>"foo.lua.old", then "foo.lua.old.2", "foo.lua.old.3"... - an earlier backup is not overwritten either.</summary>
        private static string MakeBackupPath(string targetPath)
        {
            var candidate = targetPath + BackupSuffix;
            var attempt = 2;

            while (File.Exists(candidate) || Directory.Exists(candidate))
            {
                candidate = targetPath + BackupSuffix + "." + attempt.ToString(CultureInfo.InvariantCulture);
                ++attempt;
            }

            return candidate;
        }

        private static bool IsPlugin(string relativePath)
        {
            return string.Equals(relativePath, PluginRelativePath, StringComparison.OrdinalIgnoreCase);
        }

        private static void Copy(string source, string target)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(target));
            File.Copy(source, target, overwrite: true);
        }

        private static string ToManifestPath(string relativePath)
        {
            return relativePath.Replace(Path.DirectorySeparatorChar, '/').Replace(Path.AltDirectorySeparatorChar, '/');
        }

        private static string ToFileSystemPath(string relativePath)
        {
            return relativePath.Replace('/', Path.DirectorySeparatorChar);
        }

        private static bool HashesEqual(string left, string right)
        {
            return left != null && right != null && string.Equals(left, right, StringComparison.OrdinalIgnoreCase);
        }

        private static string HashFile(string path)
        {
            using (var stream = File.OpenRead(path))
                return Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
        }

        /// <summary>
        /// FileVersionInfo's version string, or null for anything without one - a file that is not a PE image at
        /// all included, which is what a test fixture (or a corrupted download) looks like.
        /// </summary>
        private static string TryReadVersion(string path)
        {
            try
            {
                var info = FileVersionInfo.GetVersionInfo(path);
                return info?.FileVersion;
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return null;
            }
        }
    }
}
