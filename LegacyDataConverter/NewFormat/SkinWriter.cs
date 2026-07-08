using ChangerAPI.Engine;
using ChangerAPI.Utilities;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Text.Json;

namespace LegacyDataConverter.NewFormat
{
    /// <summary>
    /// Writes a legacy (deserialized) skin into the current zip+manifest.json skin format.
    /// Mirrors ChangerAPI.Engine.SkinPackager's writer - kept as an independent copy on purpose,
    /// see SkinManifest.
    /// </summary>
    internal static class SkinWriter
    {
        private const string ManifestEntryName = "manifest.json";
        private const string TexturesEntryPrefix = "textures/";
        private static readonly JsonSerializerOptions JsonOptions = new JsonSerializerOptions { WriteIndented = true };

        public static void Write(AudiosurfSkinExtended skin, string file)
        {
            var manifest = new SkinManifest
            {
                Name = skin.Name,
                Uid = skin.ID,
                Tiles = skin.Tiles?.Name,
                TilesFlyup = skin.TilesFlyup?.Name,
                Cover = skin.Cover?.Name
            };

            using (var filestream = new FileStream(file, FileMode.Create))
            using (var archive = new ZipArchive(filestream, ZipArchiveMode.Create))
            {
                var writtenEntries = new HashSet<string>();

                WriteGroup(archive, skin.Cliffs, manifest.Cliffs, writtenEntries);
                WriteGroup(archive, skin.Hits, manifest.Hits, writtenEntries);
                WriteGroup(archive, skin.Particles, manifest.Particles, writtenEntries);
                WriteGroup(archive, skin.Rings, manifest.Rings, writtenEntries);
                WriteGroup(archive, skin.SkySpheres, manifest.SkySpheres, writtenEntries);
                WriteGroup(archive, skin.SkySphereSource, manifest.SkySphereSource, writtenEntries);
                WriteGroup(archive, skin.Previews, manifest.Previews, writtenEntries);
                WriteBitmap(archive, skin.Tiles, writtenEntries);
                WriteBitmap(archive, skin.TilesFlyup, writtenEntries);
                WriteBitmap(archive, skin.Cover, writtenEntries);

                var manifestEntry = archive.CreateEntry(ManifestEntryName, CompressionLevel.Optimal);
                using (var entryStream = manifestEntry.Open())
                using (var writer = new StreamWriter(entryStream, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false)))
                {
                    writer.Write(JsonSerializer.Serialize(manifest, JsonOptions));
                }
            }
        }

        private static void WriteGroup(ZipArchive archive, ImageGroup group, List<string> manifestList, HashSet<string> writtenEntries)
        {
            if (group?.Group == null)
                return;

            foreach (var image in group.Group)
                WriteBitmap(archive, image, writtenEntries, manifestList);
        }

        private static void WriteBitmap(ZipArchive archive, NamedBitmap image, HashSet<string> writtenEntries, List<string> manifestList = null)
        {
            if (image == null || string.IsNullOrEmpty(image.Name) || image.Source == null)
                return;

            manifestList?.Add(image.Name);

            var entryName = TexturesEntryPrefix + image.Name;
            if (!writtenEntries.Add(entryName))
                return;

            var entry = archive.CreateEntry(entryName, CompressionLevel.Optimal);
            using (var memory = new MemoryStream())
            {
                // Bitmap.Save() needs a seekable stream (GDI+ writes headers after the data);
                // a ZipArchiveEntry's stream is not seekable, so buffer through memory first.
                image.Source.Save(memory, GetImageFormat(image.Name));
                memory.Position = 0;
                using (var entryStream = entry.Open())
                {
                    memory.CopyTo(entryStream);
                }
            }
        }

        private static ImageFormat GetImageFormat(string fileName)
        {
            switch (Path.GetExtension(fileName).ToLowerInvariant())
            {
                case ".png":
                    return ImageFormat.Png;
                case ".jpg":
                case ".jpeg":
                    return ImageFormat.Jpeg;
                case ".bmp":
                    return ImageFormat.Bmp;
                default:
                    return ImageFormat.Png;
            }
        }
    }
}
