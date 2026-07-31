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
    /// must not depend on). The QuickPlayer status chip is driven from PlaybackController's
    /// EntryPreparing/EntryPrepared pair rather than from TaggingStarted: this class has no "finished"
    /// counterpart to close the chip with, and the controller's pair brackets the whole preparation
    /// (including the fast no-tags path) for a manual play and an auto-advance alike.
    ///
    /// The suffix is appended to SongTitle unconditionally, which is safe because SongTitle is
    /// guaranteed to be plain text: PlaylistEntry.AdoptTagsFromTitle lifts any tag the user wrote into
    /// the file themselves out of the title and into the entry's own tag/override state, at import and
    /// again on every playlist load. That guarantee matters - a title carrying the same tag twice does
    /// not get ignored by the game, it breaks it in arbitrary ways (verified on a real run).
    ///
    /// REMAINING GAP - SongTitle is still a snapshot taken at import. A title the user edits in the
    /// file afterwards is not picked up: the mtime in BuildSignature invalidates the cached copy, but
    /// the copy is then rebuilt from the same stale SongTitle. Tags added to the file that way are
    /// invisible to Quick Player until the track is re-added. See Docs/Internal/roadmap.md.
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
