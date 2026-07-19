using System;
using System.Collections.Generic;
using System.IO;
using AudiosurfInterface;
using QuickPlayerCore.Audiosurf;
using QuickPlayerCore.MetadataParsers;

namespace QuickPlayerCore
{
    /// <summary>
    /// A tag applied to one playlist entry. Kept as a named class rather than a (Token, int?) tuple -
    /// System.Text.Json's reflection-based serializer only picks up properties, not ValueTuple's
    /// public fields, so a tuple would silently round-trip as an empty object.
    /// </summary>
    public sealed class PlaylistTag
    {
        public SongTagToken Token { get; set; }
        public int? Parameter { get; set; }
    }

    /// <summary>
    /// One song in a playlist. Id is stable and independent of FilePath/name - the same lesson
    /// learned from ColorPalette (Фаза 4.2): the same file can appear in multiple playlists, or
    /// twice in one, without entries colliding.
    /// </summary>
    public sealed class PlaylistEntry
    {
        public Guid Id { get; init; } = Guid.NewGuid();
        public string FilePath { get; set; }
        public string ArtistName { get; set; }
        public string SongTitle { get; set; }
        public string CoverPath { get; set; }

        /// <summary>Null means "no per-song override" - PlaybackController falls back to its DefaultCharacter (the transport bar's global toggle).</summary>
        public GameCharacter? Character { get; set; }

        /// <summary>Explicit per-song asconfig overrides, independent of the global Tweaker toggles.</summary>
        public Dictionary<string, bool> ConfigOverrides { get; set; } = new();

        public List<PlaylistTag> Tags { get; set; } = new();

        /// <summary>Reads artist/title off the file's own metadata via TagLib - falls back to the file name if the file can't be read.</summary>
        public static PlaylistEntry FromFile(string filePath)
        {
            var entry = new PlaylistEntry { FilePath = filePath };

            try
            {
                var tags = MetadataReader.ReadData(filePath);
                entry.ArtistName = tags.ArtistName;
                entry.SongTitle = string.IsNullOrEmpty(tags.SongName) ? Path.GetFileNameWithoutExtension(filePath) : tags.SongName;
            }
            catch
            {
                entry.SongTitle = Path.GetFileNameWithoutExtension(filePath);
            }

            return entry;
        }
    }
}
