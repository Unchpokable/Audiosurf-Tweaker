using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using QuickPlayerCore.PackedPresenters;

namespace QuickPlayerCore
{
    /// <summary>
    /// Single place that knows which audio file extensions QuickPlayer will accept - playlist file
    /// scanning, add-file dialogs and drag&amp;drop, and MetadataReader's codec lookup all go through
    /// here instead of each hardcoding their own extension list. Mutable at runtime so the format set
    /// can be adjusted without a code change once someone has actually verified the game reads a
    /// given format; persisting user-added formats across runs is a future Settings addition, not
    /// implemented here.
    /// </summary>
    public static class SupportedAudioFormats
    {
        private static readonly Dictionary<string, Codec> _extensionToCodec = new(StringComparer.OrdinalIgnoreCase)
        {
            [".mp3"] = Codec.Mp3,
            [".m4a"] = Codec.M4a,
            [".flac"] = Codec.Flac,
            [".wav"] = Codec.Wav,
        };

        public static IReadOnlyCollection<string> Extensions => _extensionToCodec.Keys;

        public static bool IsSupported(string path) => TryGetCodec(path, out _);

        public static bool TryGetCodec(string path, out Codec codec)
        {
            var extension = Path.GetExtension(path);
            if (!string.IsNullOrEmpty(extension) && _extensionToCodec.TryGetValue(extension, out codec))
                return true;

            codec = Codec.Unsupported;
            return false;
        }

        /// <summary>
        /// Registers a new extension (with its leading dot, e.g. ".ogg") experimentally at runtime.
        /// Not persisted - lasts for the current process only.
        /// </summary>
        public static void Register(string extension, Codec codec)
        {
            if (string.IsNullOrWhiteSpace(extension))
                throw new ArgumentException("Extension must not be empty", nameof(extension));

            _extensionToCodec[extension.StartsWith(".") ? extension : "." + extension] = codec;
        }

        public static IEnumerable<string> FilterSupported(IEnumerable<string> paths) =>
            paths.Where(IsSupported);
    }
}
