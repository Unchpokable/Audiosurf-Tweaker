using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Text;
using System.Text.Json;

namespace QuickPlayerCore
{
    /// <summary>What has to happen before Quick Player starts the next entry.</summary>
    public enum AdvanceTrigger
    {
        /// <summary>
        /// The moment the game reports songcomplete. The next song starts while the player is still on
        /// the leaderboard screen - the game accepts playsong from there, so the score is skipped past.
        /// </summary>
        SongComplete,

        /// <summary>
        /// songcomplete, and then the player leaving the score screen themselves (oncharacterscreen).
        /// Lets the player actually read their score, at the cost of one click per track. An
        /// oncharacterscreen that arrives without a preceding songcomplete still means "the player
        /// walked out mid-song" and stops playback, exactly as in SongComplete mode.
        /// </summary>
        CharacterScreen
    }

    /// <summary>
    /// One playlist - a name, its entries, and playback order settings. Persisted as one JSON file
    /// per playlist under QuickPlayer/Playlists/&lt;Id&gt;.json (same System.Text.Json pattern as
    /// ColorPalette/.palette). UI-agnostic like the rest of QuickPlayerCore/TweakerCore - failures
    /// are reported through OperationFailed rather than a Logger dependency, same as
    /// SkinPackager.OperationFailed/LegacyConverter.ConversionFailed.
    /// </summary>
    public sealed class Playlist
    {
        public static readonly string FileExtension = ".json";

        public Guid Id { get; init; } = Guid.NewGuid();
        public string Name { get; set; }
        // ObservableCollection rather than List - the UI binds Entries.Count reactively (playlist
        // sidebar track counts) and mutates this directly from AddTracks/RemoveCard.
        public ObservableCollection<PlaylistEntry> Entries { get; set; } = new();

        /// <summary>Advance to the next entry by index on songcomplete. Simple linear order for now - the natural extension point for shuffle/repeat later.</summary>
        public bool AutoAdvance { get; set; } = true;

        /// <summary>
        /// When the next entry starts. Orthogonal to AutoAdvance (whether to advance at all) and to the
        /// playback order - it only decides which report pulls the trigger. Default is SongComplete
        /// because it is what the field's absence deserializes to, i.e. existing playlists keep
        /// behaving as they did.
        /// </summary>
        public AdvanceTrigger AdvanceOn { get; set; } = AdvanceTrigger.SongComplete;

        public static event Action<string, Exception> OperationFailed;

        private static readonly JsonSerializerOptions _jsonOptions = new() { WriteIndented = true };

        public static Playlist Load(Guid id) => LoadFromPath(PathFor(id));

        public static IEnumerable<Playlist> LoadAll()
        {
            var directory = QuickPlayerPaths.PlaylistsDirectory;
            if (!Directory.Exists(directory))
                yield break;

            foreach (var file in Directory.EnumerateFiles(directory, "*" + FileExtension))
            {
                var playlist = LoadFromPath(file);
                if (playlist != null)
                    yield return playlist;
            }
        }

        public bool Save()
        {
            try
            {
                Directory.CreateDirectory(QuickPlayerPaths.PlaylistsDirectory);
                File.WriteAllText(PathFor(Id), JsonSerializer.Serialize(this, _jsonOptions), Encoding.UTF8);
                return true;
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to save playlist '{Name}'", ex);
                return false;
            }
        }

        public bool Delete()
        {
            try
            {
                var path = PathFor(Id);
                if (File.Exists(path))
                    File.Delete(path);

                // The tagged copies were only ever for this playlist - nothing else can reach them
                // once it is gone, and nothing else ever cleaned them up.
                TempFileTagger.DropTempDirectory(Id);
                return true;
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to delete playlist '{Name}'", ex);
                return false;
            }
        }

        private static Playlist LoadFromPath(string path)
        {
            if (!File.Exists(path))
                return null;

            try
            {
                var json = File.ReadAllText(path, Encoding.UTF8);
                var playlist = JsonSerializer.Deserialize<Playlist>(json);

                // Entries imported before titles were parsed still carry the user's own bracket tags
                // inside SongTitle, which is exactly the state that makes TempFileTagger append a
                // duplicate on playback. Migrating on load rather than only on import means an existing
                // playlist is fixed too; AdoptTagsFromTitle is idempotent, so entries that were already
                // clean are untouched and nothing is rewritten on disk until the playlist is saved for
                // some other reason.
                if (playlist?.Entries != null)
                {
                    foreach (var entry in playlist.Entries)
                        entry?.AdoptTagsFromTitle();
                }

                return playlist;
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to load playlist from '{path}'", ex);
                return null;
            }
        }

        private static string PathFor(Guid id) => Path.Combine(QuickPlayerPaths.PlaylistsDirectory, id + FileExtension);
    }
}
