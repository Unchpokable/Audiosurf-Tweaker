using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;

namespace QuickPlayerCore.CoverArt
{
    /// <summary>
    /// Looks for cover.jpg/cover.jpeg/cover.png (case-insensitive) next to the entry's audio file.
    /// The only provider implemented for now - MusicBrainzCoverArtProvider is deferred, ICoverArtProvider
    /// is the seam it will plug into later without touching callers.
    /// </summary>
    public sealed class LocalFileCoverArtProvider : ICoverArtProvider
    {
        private static readonly string[] _candidateNames = { "cover.jpg", "cover.jpeg", "cover.png" };

        public Task<string> ResolveCoverPathAsync(PlaylistEntry entry)
        {
            var directory = Path.GetDirectoryName(entry.FilePath);
            if (string.IsNullOrEmpty(directory) || !Directory.Exists(directory))
                return Task.FromResult<string>(null);

            var match = Directory.EnumerateFiles(directory)
                .FirstOrDefault(file => _candidateNames.Contains(Path.GetFileName(file), StringComparer.OrdinalIgnoreCase));

            return Task.FromResult(match);
        }
    }
}
