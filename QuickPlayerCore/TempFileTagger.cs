using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using QuickPlayerCore.Audiosurf;

namespace QuickPlayerCore
{
    /// <summary>
    /// Resolves which file path to actually hand the game for a playlist entry. Tags without an
    /// AsConfigBinding (see SongTagCatalog) have no live game command - the only way to apply them is
    /// to rewrite the title tag on a copy of the file, which the game then reads at load time. Entries
    /// with no such tags active play the original file directly, no copy made.
    ///
    /// UI-agnostic like the rest of QuickPlayerCore/TweakerCore - TaggingStarted/OperationFailed are
    /// plain events, not a StatusService call (StatusService lives in TweakerUI, which QuickPlayerCore
    /// must not depend on). The future QuickPlayer ViewModel wires TaggingStarted into
    /// StatusService.Manager.Begin(StatusToken.DiskProcess, ...), the same way SkinChangerViewModel
    /// already wires LegacyConverter.ConversionStarted.
    /// </summary>
    public static class TempFileTagger
    {
        public static event Action<PlaylistEntry> TaggingStarted;
        public static event Action<string, Exception> OperationFailed;

        public static string ResolvePlaybackPath(Guid playlistId, PlaylistEntry entry)
        {
            var activeTags = entry.Tags
                .Where(t => SongTagCatalog.Get(t.Token).AsConfigBinding is null)
                .ToList();

            if (activeTags.Count == 0)
                return entry.FilePath;

            var tempDir = QuickPlayerPaths.TempDirectory(playlistId);
            var tempPath = Path.Combine(tempDir, Path.GetFileName(entry.FilePath));
            var signature = BuildSignature(entry.FilePath, activeTags);
            var signaturePath = tempPath + ".sig";

            if (File.Exists(tempPath) && File.Exists(signaturePath) && File.ReadAllText(signaturePath) == signature)
                return tempPath;

            TaggingStarted?.Invoke(entry);

            try
            {
                Directory.CreateDirectory(tempDir);
                File.Copy(entry.FilePath, tempPath, overwrite: true);

                var file = TagLib.File.Create(tempPath);
                var suffix = string.Concat(activeTags.Select(t => SongTagCatalog.Get(t.Token).Format(t.Parameter)));
                file.Tag.Title = string.IsNullOrEmpty(suffix) ? entry.SongTitle : $"{entry.SongTitle} {suffix}";
                file.Save();

                File.WriteAllText(signaturePath, signature);
                return tempPath;
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to prepare tagged copy of '{entry.FilePath}'", ex);
                return entry.FilePath; // degrade to the original file rather than fail playback entirely
            }
        }

        private static string BuildSignature(string filePath, List<PlaylistTag> activeTags)
        {
            var mtime = File.GetLastWriteTimeUtc(filePath).Ticks;
            var tagPart = string.Join(",", activeTags.Select(t => $"{t.Token}:{t.Parameter}"));
            return $"{mtime}|{tagPart}";
        }
    }
}
