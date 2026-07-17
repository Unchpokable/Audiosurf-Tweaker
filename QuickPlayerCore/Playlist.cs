using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Text;
using System.Text.Json;

namespace QuickPlayerCore
{
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
                return JsonSerializer.Deserialize<Playlist>(json);
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
