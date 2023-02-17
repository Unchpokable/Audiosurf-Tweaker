using QuickPlayerCore.PackedPresenters;
using System;

namespace QuickPlayerCore
{
    /// <summary>
    /// Dataclass that presents generic tags for any audio file, like song name, artist name, album, duration, genre, and sampling parameters
    /// </summary>
    public class GenericTagsContainer
    {
        public string SongName { get; set; }
        public string ArtistName { get; set; }
        public string Album { get; set; }
        public TimeSpan Duration { get; set; }
        public SamplingParams SamplingParams { get; set; }
        public bool IsLossless { get; set; }
    }
}
