using TweakerCore.Utilities;
using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.Json;
using SkiaSharp;

namespace TweakerCore.Engine
{
    using Env = EnvironmentalVeriables;

    public static class SkinPackager
    {
        public static readonly string SkinExtension = @".tasp";
        public static string OutputPath { get; set; }

        public static event Action<string, Exception> OperationFailed;

        private const string ManifestEntryName = "manifest.json";
        private const string TexturesEntryPrefix = "textures/";
        private const int JpegEncodeQuality = 95;

        private static string[] _texturesNames;

        private static readonly string _defaultOutput = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
        private static readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions { WriteIndented = true };

        static SkinPackager()
        {
            _texturesNames = new[]
            {
                "cliff-1.png", "cliff-2.png", "cliff2-1.png", "hit1.png", "hit2.png",
                "particles1.png", "particles2.png", "particles3.png", "ring1A.png", "ring1B.png", "ring2A.jpg", "ring2B.jpg",
                "Skyshpere_Black.png", "Skysphere_Grey.png", "Skyshphere_White.png",
                "tileflyup.png", "tiles.png"
            };
        }

        public static bool Compile(AudiosurfSkinExtended skin)
        {
            return CompileToFile(skin, (OutputPath ?? _defaultOutput) + @"\\" + skin.Name + SkinExtension);
        }

        public static bool CompileToPath(AudiosurfSkinExtended skin, string path)
        {
            return CompileToFile(skin, path + @"\\" + skin.Name + SkinExtension);
        }

        public static bool CompileToFile(AudiosurfSkinExtended skin, string file)
        {
            try
            {
                WriteSkinArchive(skin, file);
                return true;
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to compile skin '{skin?.Name}' to '{file}'", ex);
                return false;
            }
        }

        public static bool RewriteCompile(AudiosurfSkinExtended skin, string path)
        {
            return CompileToFile(skin, path);
        }

        public static AudiosurfSkinExtended Decompile(string path)
        {
            if (!File.Exists(path))
                return null;

            if (new[] { Env.LegacySkinExtention, Env.ActualSkinExtention }.All(ext => ext != Path.GetExtension(path)))
                return null;

            try
            {
                if (!IsZipArchive(path) && !LegacyConverter.TryConvert(path))
                    return null;

                return ReadSkinArchive(path);
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to decompile skin from '{path}'", ex);
                return null;
            }
        }


        public static AudiosurfSkinExtended CreateSkinFromFolder(string path)
        {
            var result = new AudiosurfSkinExtended();

            string[] allPictures = Directory.GetFiles(path);
            if (!allPictures.Any(fileName => _texturesNames.Contains(Path.GetFileName(fileName))))
                return null;

            result.Cliffs = GetAllImagesByNameMask("cliffs", Env.CliffImagesMask, path);
            result.Hits = GetAllImagesByNameMask("hits", Env.HitImageMask, path);
            result.Particles = GetAllImagesByNameMask("particles", Env.ParticlesImageMask, path);
            result.Rings = GetAllImagesByNameMask("rings", Env.RingsImageMask, path);
            result.SkySpheres = GetAllImagesByNameMask("skysphere", Env.SkysphereImagesMask, path);

            ImageGroup tiles = GetAllImagesByNameMask("tiles", "tiles.png", path);
            ImageGroup tileflyup = GetAllImagesByNameMask("tiles flyup", "tileflyup.png", path);
            if (Directory.Exists(path + @"\Screenshots"))
            {
                result.Previews = GetAllImagesByNameMask("Screenshots", "screenshot", path + @"\Screenshots");
                result.Cover = result.Previews?.Group?.FirstOrDefault();
            }

            if (tiles.Group.Count > 1 || tileflyup.Group.Count > 1)
            {
                return null;
            }

            result.Tiles = new NamedBitmap("tiles.png", (SKBitmap)tiles);
            result.TilesFlyup = new NamedBitmap("tileflyup.png", (SKBitmap)tileflyup);
            return result;
        }

        private static ImageGroup GetAllImagesByNameMask(string groupName, string nameMask, string path)
        {
            var group = new ImageGroup(groupName);

            string[] allFiles = Directory.GetFiles(path);

            foreach(var file in allFiles)
            {
                var origName = Path.GetFileName(file);
                var fname = origName.ToLower();
                var fileExt = Path.GetExtension(fname);
                if (fname.Contains(nameMask) && new[] { ".png", ".jpg" }.Any(x => x == fileExt))
                {
                    var image = new NamedBitmap(origName, SKBitmap.Decode(file));
                    group.AddImage(image);
                }
            }

            return group;
        }

        #region New archive format (zip + manifest.json)

        private static bool IsZipArchive(string path)
        {
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read))
            {
                if (stream.Length < 4)
                    return false;

                var header = new byte[4];
                stream.Read(header, 0, 4);
                return header[0] == 0x50 && header[1] == 0x4B && (header[2] == 0x03 || header[2] == 0x05 || header[2] == 0x07);
            }
        }

        private static void WriteSkinArchive(AudiosurfSkinExtended skin, string file)
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
                var writtenEntries = new System.Collections.Generic.HashSet<string>();

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
                    writer.Write(JsonSerializer.Serialize(manifest, _jsonOptions));
                }
            }
        }

        private static void WriteGroup(ZipArchive archive, ImageGroup group, System.Collections.Generic.List<string> manifestList, System.Collections.Generic.HashSet<string> writtenEntries)
        {
            if (group?.Group == null)
                return;

            foreach (var image in group.Group)
                WriteBitmap(archive, image, writtenEntries, manifestList);
        }

        private static void WriteBitmap(ZipArchive archive, NamedBitmap image, System.Collections.Generic.HashSet<string> writtenEntries, System.Collections.Generic.List<string> manifestList = null)
        {
            if (image == null || string.IsNullOrEmpty(image.Name))
                return;

            manifestList?.Add(image.Name);

            var entryName = TexturesEntryPrefix + image.Name;
            if (!writtenEntries.Add(entryName))
                return; // same filename already written as part of another group (e.g. shared textures)

            var entry = archive.CreateEntry(entryName, CompressionLevel.Optimal);
            using (var skImage = SKImage.FromBitmap((SKBitmap)image))
            using (var data = skImage.Encode(GetImageFormat(image.Name), JpegEncodeQuality))
            using (var entryStream = entry.Open())
            {
                // SKData is fully materialized in memory, so writing straight into the
                // non-seekable zip entry stream is fine (unlike GDI+ Bitmap.Save was).
                data.SaveTo(entryStream);
            }
        }

        private static AudiosurfSkinExtended ReadSkinArchive(string file)
        {
            using (var filestream = new FileStream(file, FileMode.Open, FileAccess.Read))
            using (var archive = new ZipArchive(filestream, ZipArchiveMode.Read))
            {
                var manifestEntry = archive.GetEntry(ManifestEntryName);
                if (manifestEntry == null)
                    return null;

                SkinManifest manifest;
                using (var entryStream = manifestEntry.Open())
                using (var reader = new StreamReader(entryStream, Encoding.UTF8))
                {
                    manifest = JsonSerializer.Deserialize<SkinManifest>(reader.ReadToEnd());
                }

                return new AudiosurfSkinExtended
                {
                    Name = manifest.Name,
                    Source = file,
                    Cliffs = ReadGroup(archive, "cliffs", manifest.Cliffs),
                    Hits = ReadGroup(archive, "hits", manifest.Hits),
                    Particles = ReadGroup(archive, "particles", manifest.Particles),
                    Rings = ReadGroup(archive, "rings", manifest.Rings),
                    SkySpheres = ReadGroup(archive, "skysphere", manifest.SkySpheres),
                    SkySphereSource = ReadGroup(archive, "skysphere source", manifest.SkySphereSource),
                    Previews = ReadGroup(archive, "Screenshots", manifest.Previews),
                    Tiles = ReadBitmap(archive, manifest.Tiles),
                    TilesFlyup = ReadBitmap(archive, manifest.TilesFlyup),
                    Cover = ReadBitmap(archive, manifest.Cover)
                };
            }
        }

        private static ImageGroup ReadGroup(ZipArchive archive, string groupName, System.Collections.Generic.List<string> fileNames)
        {
            var group = new ImageGroup(groupName);
            if (fileNames == null)
                return group;

            foreach (var fileName in fileNames)
            {
                var bitmap = ReadBitmap(archive, fileName);
                if (bitmap != null)
                    group.AddImage(bitmap);
            }

            return group;
        }

        private static NamedBitmap ReadBitmap(ZipArchive archive, string fileName)
        {
            if (string.IsNullOrEmpty(fileName))
                return null;

            var entry = archive.GetEntry(TexturesEntryPrefix + fileName);
            if (entry == null)
                return null;

            using (var entryStream = entry.Open())
            using (var memory = new MemoryStream())
            {
                entryStream.CopyTo(memory);
                memory.Position = 0;
                return new NamedBitmap(fileName, SKBitmap.Decode(memory));
            }
        }

        private static SKEncodedImageFormat GetImageFormat(string fileName)
        {
            switch (Path.GetExtension(fileName).ToLowerInvariant())
            {
                case ".png":
                    return SKEncodedImageFormat.Png;
                case ".jpg":
                case ".jpeg":
                    return SKEncodedImageFormat.Jpeg;
                // Skia can't encode BMP; the skin masks only admit .png/.jpg anyway.
                default:
                    return SKEncodedImageFormat.Png;
            }
        }

        #endregion
    }
}
