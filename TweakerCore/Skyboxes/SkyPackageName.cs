using System;
using System.Linq;

namespace TweakerCore.Skyboxes
{
    /// <summary>
    /// A sky package's name (Docs/Internal/sky-manager-roadmap.md §5.1): the "name" of its Config.json, which is also
    /// the file it is installed as - "My Sky" goes into the game as My Sky.sky. That file name is what everybody sees:
    /// the overlay lists a package by it, and the plugin keys the player's settings by it (SkyConfigs\My Sky.json).
    ///
    /// So the name follows the rules of a file name. They are the plugin's rules for an entry of the Skyboxes folder
    /// (sky_paths, sky_vfs: one plain name - no separator, no drive colon, no control character, not "." or "..",
    /// no device name, nothing a Windows path would silently trim) plus the characters Windows refuses in a file
    /// name. Case does not matter - the plugin compares names on disk without it.
    /// </summary>
    public static class SkyPackageName
    {
        public const int MaxLength = 64;

        private static readonly char[] Forbidden = { '\\', '/', ':', '*', '?', '"', '<', '>', '|' };

        private static readonly string[] Devices =
        {
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
        };

        /// <summary>What is wrong with <paramref name="name"/> as a package name, in a sentence; null when it will do.</summary>
        public static string Problem(string name)
        {
            if (string.IsNullOrWhiteSpace(name))
                return "Must not be empty.";
            if (name.Length > MaxLength)
                return $"At most {MaxLength} characters - it is a file name.";
            if (name.Any(char.IsControl))
                return "Must be a single line.";
            if (name.IndexOfAny(Forbidden) >= 0)
                return "It is a file name, so none of \\ / : * ? \" < > |";
            if (name != name.Trim())
                return "No spaces at the start or the end.";
            if (name.EndsWith(".", StringComparison.Ordinal))
                return "Must not end with a dot - Windows drops it from a file name.";
            if (name == "." || name == "..")
                return "Must be a name, not \".\" or \"..\".";

            var stem = name.Split('.')[0];
            if (Devices.Any(device => string.Equals(stem, device, StringComparison.OrdinalIgnoreCase)))
                return $"{stem} is a device name in Windows and cannot be a file name.";

            if (name.EndsWith(SkyPaths.PackageExtension, StringComparison.OrdinalIgnoreCase))
                return $"Leave out {SkyPaths.PackageExtension} - it is added when the sky is installed.";
            return null;
        }

        /// <summary>"My Sky" -> "My Sky.sky"; null when the name will not do.</summary>
        public static string FileName(string name)
        {
            return Problem(name) == null ? name + SkyPaths.PackageExtension : null;
        }
    }
}
