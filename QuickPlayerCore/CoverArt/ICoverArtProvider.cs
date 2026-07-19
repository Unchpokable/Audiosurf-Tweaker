using System.Threading.Tasks;

namespace QuickPlayerCore.CoverArt
{
    /// <summary>
    /// Resolves a local file path to a cover image for a playlist entry, or null if none is found.
    /// Always a local path (never raw bytes) - a remote provider (MusicBrainz, deferred) would
    /// download and cache to disk before returning, same as the local provider just finding a file.
    /// </summary>
    public interface ICoverArtProvider
    {
        Task<string> ResolveCoverPathAsync(PlaylistEntry entry);
    }
}
