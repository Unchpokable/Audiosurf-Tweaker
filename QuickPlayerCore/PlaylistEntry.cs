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
            string rawTitle;

            try
            {
                var tags = MetadataReader.ReadData(filePath);
                entry.ArtistName = tags.ArtistName;
                rawTitle = string.IsNullOrEmpty(tags.SongName) ? Path.GetFileNameWithoutExtension(filePath) : tags.SongName;
            }
            catch
            {
                rawTitle = Path.GetFileNameWithoutExtension(filePath);
            }

            entry.SongTitle = rawTitle;
            entry.AdoptTagsFromTitle();

            if (string.IsNullOrWhiteSpace(entry.SongTitle))
                entry.SongTitle = SongTitleTagParser.Parse(Path.GetFileNameWithoutExtension(filePath)).Title;

            return entry;
        }

        /// <summary>
        /// Moves any Audiosurf tag the user had written into the title themselves out of SongTitle and
        /// into this entry's own tag/override state, so the title Quick Player stores is plain text.
        /// TempFileTagger composes title + tags on playback, so leaving them in would append a second
        /// copy of a tag that is already there - and a duplicated tag breaks the game outright.
        ///
        /// Tags that map to a live asconfig value (Sidewinder/BankingCamera) go to ConfigOverrides
        /// rather than Tags: those two are shown as "Mods" in the UI and TagOptionViewModel hides them
        /// from the Tags list, so an entry carrying one in Tags would have it silently dropped the
        /// first time the user edited anything on that track (TrackCardViewModel rebuilds Tags from the
        /// visible options).
        ///
        /// Idempotent - a title with nothing recognisable in it is left exactly as it was, so running
        /// this over already-imported entries cannot damage them.
        /// </summary>
        internal void AdoptTagsFromTitle()
        {
            var parsed = SongTitleTagParser.Parse(SongTitle);
            if (parsed.Tags.Count == 0)
                return;

            SongTitle = parsed.Title;
            Tags ??= new List<PlaylistTag>();
            ConfigOverrides ??= new Dictionary<string, bool>();

            foreach (var tag in parsed.Tags)
            {
                var definition = SongTagCatalog.Get(tag.Token);
                if (definition.AsConfigBinding is { } binding)
                {
                    ConfigOverrides[binding.ConfigKey] = binding.Value;
                    continue;
                }

                if (!Tags.Exists(existing => existing.Token == tag.Token))
                    Tags.Add(tag);
            }
        }
    }
}
