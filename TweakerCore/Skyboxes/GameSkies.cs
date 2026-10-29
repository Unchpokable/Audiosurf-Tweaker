using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using SkiaSharp;
using TweakerCore.PluginInstall;

namespace TweakerCore.Skyboxes
{
    /// <summary>One entry of the game's Skyboxes folder.</summary>
    public sealed class GameSky
    {
        /// <summary>The entry's own name - the id module.json and the protocol use ("My Sky.sky").</summary>
        public string Id { get; init; }

        /// <summary>What the overlay shows: the name without the extension for a package, a file and a shader.</summary>
        public string DisplayName { get; init; }

        public SkyKind Kind { get; init; }

        /// <summary>The size of one cube face, read from an image header; 0 when unknown or not an image.</summary>
        public int FaceSize { get; init; }

        /// <summary>A folder on disk, as opposed to a file - a package in folder form or a folder of faces.</summary>
        public bool IsFolder { get; init; }

        /// <summary>Came with Tweaker: in the plugin bundle or recorded in .deploy.json.</summary>
        public bool IsShipped { get; init; }

        public string FullPath { get; init; }
    }

    /// <summary>
    /// The skies in the game's Skyboxes folder, without the plugin (Docs/Internal/sky-manager-roadmap.md §5.4) - what
    /// the Skyboxes tab lists while the game is not running or not connected. Connected, the plugin's own catalog
    /// takes over, and it is the only one that knows the skies built into the DLL.
    ///
    /// The rules are the plugin's (sky_catalog.cxx: add_directory_entries), in the same order - change one, change
    /// the other:
    ///   - a folder with Config.json in it is a package - a manifest wins over six images that happen to be there;
    ///   - a folder with a file named like a cube face (posx/px/right and the rest) is a folder of faces;
    ///   - a file ending in .sky is a package (the zip form), listed without being opened;
    ///   - a .hlsl is a shader, listed without being compiled;
    ///   - an image whose header says 4:3 (a cross) or 2:1 (a panorama) is a cube map; anything else is skipped.
    /// Sorted like the catalog: by kind, then by name.
    ///
    /// One known difference: the plugin reads image headers with stb_image, this with SkiaSharp plus a Radiance .hdr
    /// reader, so a TGA or PSD cube map is listed only when the game is connected.
    /// </summary>
    public static class GameSkies
    {
        private static readonly string[][] FaceStems =
        {
            new[] { "posx", "px", "right" }, new[] { "negx", "nx", "left" }, new[] { "posy", "py", "top" },
            new[] { "negy", "ny", "bottom" }, new[] { "posz", "pz", "front" }, new[] { "negz", "nz", "back" }
        };

        public static IReadOnlyList<GameSky> Scan(string engineDirectory, string payloadDirectory)
        {
            var root = SkyPaths.SkyboxesPath(engineDirectory);
            if (!Directory.Exists(root))
                return Array.Empty<GameSky>();

            var manifest = PluginInstallation.IsValidEngineFolder(engineDirectory) ? PluginDeployManifest.Load(engineDirectory) : new PluginDeployManifest();
            var result = new List<GameSky>();

            foreach (var path in SafeEntries(root))
            {
                var id = Path.GetFileName(path);
                var stem = Path.GetFileNameWithoutExtension(path);
                var extension = Path.GetExtension(path);
                var shipped = IsShipped(manifest, payloadDirectory, id);

                if (Directory.Exists(path))
                {
                    if (File.Exists(Path.Combine(path, SkyProjects.ManifestFileName)))
                    {
                        result.Add(new GameSky { Id = id, DisplayName = stem, Kind = SkyKind.Package, IsFolder = true, IsShipped = shipped, FullPath = path });
                        continue;
                    }
                    if (TryFaceDirectory(path, out var faceSize))
                        result.Add(new GameSky { Id = id, DisplayName = id, Kind = SkyKind.FaceDir, FaceSize = faceSize, IsFolder = true, IsShipped = shipped, FullPath = path });
                    continue;
                }

                if (string.Equals(extension, SkyPaths.PackageExtension, StringComparison.OrdinalIgnoreCase))
                {
                    result.Add(new GameSky { Id = id, DisplayName = stem, Kind = SkyKind.Package, IsShipped = shipped, FullPath = path });
                    continue;
                }
                if (string.Equals(extension, ".hlsl", StringComparison.OrdinalIgnoreCase))
                {
                    result.Add(new GameSky { Id = id, DisplayName = stem, Kind = SkyKind.ShaderFile, IsShipped = shipped, FullPath = path });
                    continue;
                }

                if (!TryReadImageSize(path, out var width, out var height))
                    continue;
                var size = FaceSizeOfSingleImage(width, height);
                if (size > 0)
                    result.Add(new GameSky { Id = id, DisplayName = stem, Kind = SkyKind.File, FaceSize = size, IsShipped = shipped, FullPath = path });
            }

            return result
                .OrderBy(sky => (int)sky.Kind)
                .ThenBy(sky => sky.DisplayName, StringComparer.Ordinal)
                .ToList();
        }

        /// <summary>
        /// Whether a sky of that name came with Tweaker: it is in the bundle, or the plugin's installation recorded it
        /// (a file of that name, or any file under a folder of that name).
        /// </summary>
        public static bool IsShipped(string engineDirectory, string payloadDirectory, string id)
        {
            var manifest = PluginInstallation.IsValidEngineFolder(engineDirectory) ? PluginDeployManifest.Load(engineDirectory) : new PluginDeployManifest();
            return IsShipped(manifest, payloadDirectory, id);
        }

        private static bool IsShipped(PluginDeployManifest manifest, string payloadDirectory, string id)
        {
            if (!string.IsNullOrWhiteSpace(payloadDirectory))
            {
                var bundled = Path.Combine(payloadDirectory, PluginInstallation.StuffFolderName, SkyPaths.ReplacerFolderName, SkyPaths.SkyboxesFolderName, id);
                if (File.Exists(bundled) || Directory.Exists(bundled))
                    return true;
            }

            var key = SkyPaths.SkyboxesRelativeFolder + id;
            return manifest.Files != null
                   && manifest.Files.Keys.Any(path => string.Equals(path, key, StringComparison.OrdinalIgnoreCase)
                                                      || path.StartsWith(key + "/", StringComparison.OrdinalIgnoreCase));
        }

        /// <summary>A quarter of the width for both accepted single-image layouts: a 4:3 cross and a 2:1 panorama; 0 for anything else.</summary>
        internal static int FaceSizeOfSingleImage(int width, int height)
        {
            if (width <= 0 || height <= 0)
                return 0;
            return width == height * 2 || width * 3 == height * 4 ? width / 4 : 0;
        }

        /// <summary>
        /// A folder of faces when at least one file in it is named like a face - all six are not required, so a
        /// half-finished set is listed and the loader says which face is missing. The size is the first face's, when
        /// it is square.
        /// </summary>
        private static bool TryFaceDirectory(string directory, out int faceSize)
        {
            faceSize = 0;
            foreach (var file in SafeFiles(directory))
            {
                var stem = Path.GetFileNameWithoutExtension(file);
                if (!FaceStems.Any(spellings => spellings.Any(s => string.Equals(s, stem, StringComparison.OrdinalIgnoreCase))))
                    continue;
                if (TryReadImageSize(file, out var width, out var height) && width == height)
                    faceSize = width;
                return true;
            }
            return false;
        }

        /// <summary>An image's size from its header alone - a folder of 2048-pixel faces is kilobytes to list, not a decode.</summary>
        internal static bool TryReadImageSize(string path, out int width, out int height)
        {
            width = 0;
            height = 0;
            try
            {
                if (TryReadRadianceSize(path, out width, out height))
                    return true;

                using var stream = File.OpenRead(path);
                using var codec = SKCodec.Create(stream);
                if (codec == null)
                    return false;
                width = codec.Info.Width;
                height = codec.Info.Height;
                return width > 0 && height > 0;
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException || exception is ArgumentException)
            {
                return false;
            }
        }

        /// <summary>Radiance .hdr: a "#?" header, a blank line, then "-Y height +X width".</summary>
        private static bool TryReadRadianceSize(string path, out int width, out int height)
        {
            width = 0;
            height = 0;
            using var stream = File.OpenRead(path);
            var head = new byte[2048];
            var read = stream.Read(head, 0, head.Length);
            if (read < 2 || head[0] != (byte)'#' || head[1] != (byte)'?')
                return false;

            var lines = Encoding.ASCII.GetString(head, 0, read).Split('\n');
            foreach (var line in lines)
            {
                var parts = line.Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length == 4 && (parts[0] == "-Y" || parts[0] == "+Y") && (parts[2] == "+X" || parts[2] == "-X")
                    && int.TryParse(parts[1], out height) && int.TryParse(parts[3], out width))
                {
                    return width > 0 && height > 0;
                }
            }
            return false;
        }

        private static IEnumerable<string> SafeEntries(string directory)
        {
            try
            {
                return Directory.EnumerateFileSystemEntries(directory).ToList();
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return Array.Empty<string>();
            }
        }

        private static IEnumerable<string> SafeFiles(string directory)
        {
            try
            {
                return Directory.EnumerateFiles(directory).ToList();
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                return Array.Empty<string>();
            }
        }
    }
}
