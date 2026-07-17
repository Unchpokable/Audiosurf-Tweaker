using System;
using System.IO;

namespace QuickPlayerCore
{
    /// <summary>
    /// Resolves QuickPlayer's on-disk folders relative to the running executable. Uses
    /// Environment.ProcessPath rather than Assembly.Location/AppDomain.BaseDirectory - both resolve
    /// wrong (empty, or a %TEMP% self-extraction folder) under a single-file publish, a bug already
    /// hit once and fixed the same way for Skins/LegacyDataConverter (see the "Финальная полировка"
    /// section of the roadmap). Duplicated here rather than shared with TweakerUI's own AppDirectory
    /// helper so QuickPlayerCore doesn't have to depend on TweakerUI.
    /// </summary>
    internal static class QuickPlayerPaths
    {
        public static string BaseDirectory => Path.GetDirectoryName(Environment.ProcessPath) ?? AppContext.BaseDirectory;

        public static string RootDirectory => Path.Combine(BaseDirectory, "QuickPlayer");

        public static string PlaylistsDirectory => Path.Combine(RootDirectory, "Playlists");

        public static string TempDirectory(Guid playlistId) => Path.Combine(RootDirectory, "Temp", playlistId.ToString());

        public static string CoverCacheDirectory => Path.Combine(RootDirectory, "CoverCache");
    }
}
