using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
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
    /// The copy is content-addressed and published atomically, which is what makes PlaylistPrewarmer
    /// safe: the name encodes source path, mtime and tag set, so a prepared copy is either present and
    /// correct or absent, never stale and never half-written. Callers preparing the same entry
    /// concurrently duplicate a little work and converge on the same file instead of fighting over it.
    ///
    /// REMAINING GAP - SongTitle is still a snapshot taken at import. A title the user edits in the
    /// file afterwards is not picked up: the mtime in the name invalidates the cached copy, but the
    /// copy is then rebuilt from the same stale SongTitle. Tags added to the file that way are
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
            var tempPath = Path.Combine(tempDir, BuildFileName(entry.FilePath, activeTags));

            // The name is the signature, so its presence is the whole cache check - and because the
            // file is only ever moved into place complete, an interrupted run leaves no half-copy that
            // a later run would mistake for a good one.
            if (File.Exists(tempPath))
                return tempPath;

            TaggingStarted?.Invoke(entry);

            // Staged under a name unique to this call: two threads preparing the same entry write to
            // separate files and one of them wins the publish below, rather than one overwriting the
            // other's copy while it is still being tagged. The original extension has to stay last -
            // TagLib picks its format handler from it, and a name ending in ".part" is a format it
            // refuses outright.
            var staging = Path.Combine(
                tempDir,
                Path.GetFileNameWithoutExtension(tempPath)
                    + ".part" + Guid.NewGuid().ToString("N")
                    + Path.GetExtension(tempPath));

            try
            {
                Directory.CreateDirectory(tempDir);
                File.Copy(entry.FilePath, staging, overwrite: true);

                var file = TagLib.File.Create(staging);
                var suffix = string.Concat(activeTags.Select(t => SongTagCatalog.Get(t.Token).Format(t.Parameter)));
                file.Tag.Title = string.IsNullOrEmpty(suffix) ? entry.SongTitle : $"{entry.SongTitle} {suffix}";
                file.Save();

                try
                {
                    File.Move(staging, tempPath);
                }
                catch (IOException) when (File.Exists(tempPath))
                {
                    // Lost the race; the published file is byte-for-byte what this call was building.
                    // Never overwrite here - the winner may already be open in the game.
                    TryDelete(staging);
                }

                return tempPath;
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to prepare tagged copy of '{entry.FilePath}'", ex);
                TryDelete(staging);
                return entry.FilePath; // degrade to the original file rather than fail playback entirely
            }
        }

        /// <summary>
        /// Deletes everything in the playlist's temp folder that isn't in <paramref name="keep"/>.
        /// Only safe to call once a full pass has established what the playlist actually needs - see
        /// PlaylistPrewarmer, the only caller.
        /// </summary>
        internal static void PruneTempDirectory(Guid playlistId, ISet<string> keep)
        {
            var tempDir = QuickPlayerPaths.TempDirectory(playlistId);
            if (!Directory.Exists(tempDir))
                return;

            try
            {
                foreach (var path in Directory.EnumerateFiles(tempDir))
                {
                    if (!keep.Contains(path))
                        TryDelete(path);
                }
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to clean up the temp folder for playlist '{playlistId}'", ex);
            }
        }

        /// <summary>Removes a playlist's whole temp folder, for when the playlist itself is deleted.</summary>
        internal static void DropTempDirectory(Guid playlistId)
        {
            try
            {
                var tempDir = QuickPlayerPaths.TempDirectory(playlistId);
                if (Directory.Exists(tempDir))
                    Directory.Delete(tempDir, recursive: true);
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to remove the temp folder for playlist '{playlistId}'", ex);
            }
        }

        private static void TryDelete(string path)
        {
            try
            {
                if (File.Exists(path))
                    File.Delete(path);
            }
            catch
            {
                // A leftover in the temp folder is harmless - the next prune sweeps it up, and failing
                // playback over a file we could not delete would be a worse trade.
            }
        }

        // Everything that decides the copy's contents goes into the hash: the source path (two folders
        // can hold "track.mp3"), its mtime, and the tag set with parameters. Extension is preserved
        // because the game picks its decoder from it. SHA256 rather than string.GetHashCode - the
        // latter is randomized per process, so the name would change on every restart and no cached
        // copy would ever be reused.
        private static string BuildFileName(string filePath, List<PlaylistTag> activeTags)
        {
            var mtime = File.GetLastWriteTimeUtc(filePath).Ticks;
            var tagPart = string.Join(",", activeTags.Select(t => $"{t.Token}:{t.Parameter}"));
            var signature = $"{filePath}|{mtime}|{tagPart}";

            var hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(signature)), 0, 8).ToLowerInvariant();
            return Path.GetFileNameWithoutExtension(filePath) + "." + hash + Path.GetExtension(filePath);
        }
    }
}
