using TweakerCore.Utilities;
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using SkiaSharp;

namespace TweakerCore.Engine
{
    using Env = EnvironmentalVeriables;

    public static class SkinPackager
    {
        public static event Action<string, Exception> OperationFailed;

        private const string ManifestEntryName = "manifest.json";
        private const string TexturesEntryPrefix = "textures/";
        private const int JpegEncodeQuality = 95;

        private static readonly string[] _texturesNames =
        {
            "cliff-1.png", "cliff-2.png", "cliff2-1.png", "hit1.png", "hit2.png",
            "particles1.png", "particles2.png", "particles3.png", "ring1A.png", "ring1B.png", "ring2A.jpg", "ring2B.jpg",
            "Skyshpere_Black.png", "Skysphere_Grey.png", "Skyshphere_White.png",
            "tileflyup.png", "tiles.png"
        };

        private static readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions { WriteIndented = true };

        public static bool CompileToPath(AudiosurfSkinExtended skin, string path)
        {
            return CompileToFile(skin, Path.Combine(path, skin.Name + Env.ActualSkinExtention));
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

        // Decodes EVERY texture of the skin - a 4k skysphere alone is 64 MB of pixels. Only for what needs
        // the whole skin (install, export); the Skin Changer list and its screenshots have ReadName and
        // ReadPreviews.
        public static AudiosurfSkinExtended Decompile(string path)
        {
            if (!IsSkinFile(path))
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


        // The skin's name and nothing else: reads manifest.json only. Also what tells a skin file from any
        // other file - null for anything that isn't one. Legacy files are converted in place first, same as
        // Decompile.
        public static string ReadName(string path)
        {
            if (!IsSkinFile(path))
                return null;

            try
            {
                if (!IsZipArchive(path) && !LegacyConverter.TryConvert(path))
                    return null;

                using (var archive = ZipFile.OpenRead(path))
                    return ReadManifest(archive)?.Name;
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to read skin name from '{path}'", ex);
                return null;
            }
        }

        // Decodes the preview screenshots only - none of the skin's textures - one at a time: each is handed
        // to convert and disposed right after, so at most one full-size screenshot is decoded at any moment.
        // Whatever convert returns is the caller's to own (and dispose). An entry that fails to decode is
        // skipped.
        public static List<T> ReadPreviews<T>(string path, Func<SKBitmap, T> convert)
        {
            var result = new List<T>();
            if (!IsSkinFile(path))
                return result;

            try
            {
                if (!IsZipArchive(path) && !LegacyConverter.TryConvert(path))
                    return result;

                using (var archive = ZipFile.OpenRead(path))
                {
                    var manifest = ReadManifest(archive);
                    foreach (var fileName in manifest?.Previews ?? new List<string>())
                    {
                        using (var bitmap = DecodeEntry(archive, fileName))
                        {
                            if (bitmap != null)
                                result.Add(convert(bitmap));
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to read screenshots from '{path}'", ex);
            }

            return result;
        }


        // Renames a skin without decoding it: only manifest.json is rewritten, every texture entry is
        // carried over as its original compressed bytes (ZipArchiveMode.Update never recompresses an
        // entry nobody opened). The old Decompile + CompileToPath route re-encoded every texture - seconds
        // of work for one string, plus a generational quality loss on the JPEG rings each time.
        // The new file is fully written before the old one is touched, so a failure midway leaves the
        // original skin intact. The caller is responsible for newFile not belonging to another skin.
        public static bool RenameSkin(string file, string newName, string newFile)
        {
            var tempFile = newFile + ".renaming";
            try
            {
                if (!IsZipArchive(file) && !LegacyConverter.TryConvert(file))
                    return false;

                File.Copy(file, tempFile, overwrite: true);
                using (var archive = ZipFile.Open(tempFile, ZipArchiveMode.Update))
                {
                    var manifestEntry = archive.GetEntry(ManifestEntryName);
                    if (manifestEntry == null)
                        throw new InvalidDataException($"'{file}' has no {ManifestEntryName}");

                    SkinManifest manifest;
                    using (var entryStream = manifestEntry.Open())
                    using (var reader = new StreamReader(entryStream, Encoding.UTF8))
                    {
                        manifest = JsonSerializer.Deserialize<SkinManifest>(reader.ReadToEnd());
                    }

                    manifest.Name = newName;
                    manifestEntry.Delete();

                    var newEntry = archive.CreateEntry(ManifestEntryName, CompressionLevel.Optimal);
                    using (var entryStream = newEntry.Open())
                    using (var writer = new StreamWriter(entryStream, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false)))
                    {
                        writer.Write(JsonSerializer.Serialize(manifest, _jsonOptions));
                    }
                }

                // Same path (the file name didn't follow the skin name, or only the case changed):
                // the old file has to go first, there is no other way to free the name.
                if (string.Equals(Path.GetFullPath(file), Path.GetFullPath(newFile), StringComparison.OrdinalIgnoreCase))
                {
                    File.Delete(file);
                    File.Move(tempFile, newFile);
                }
                else
                {
                    File.Move(tempFile, newFile);
                    try
                    {
                        File.Delete(file);
                    }
                    catch
                    {
                        // Old file is locked: undo, otherwise the skin now exists twice under two names.
                        File.Delete(newFile);
                        throw;
                    }
                }

                return true;
            }
            catch (Exception ex)
            {
                OperationFailed?.Invoke($"Failed to rename skin '{file}' to '{newName}'", ex);
                try
                {
                    if (File.Exists(tempFile))
                        File.Delete(tempFile);
                }
                catch
                {
                    // best-effort cleanup, the original failure is what gets reported
                }
                return false;
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

            // Previously only rejected "more than one match" - a folder with neither file wrapped a
            // null SKBitmap into the result instead, which only surfaced later as a generic
            // "failed to compile" from Compile's SKImage.FromBitmap(null) rather than a clear cause here.
            if (tiles.Group.Count != 1 || tileflyup.Group.Count != 1)
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

        private static bool IsSkinFile(string path)
        {
            return File.Exists(path)
                && new[] { Env.LegacySkinExtention, Env.ActualSkinExtention }.Any(ext => ext.Equals(Path.GetExtension(path), StringComparison.OrdinalIgnoreCase));
        }

        private static SkinManifest ReadManifest(ZipArchive archive)
        {
            var manifestEntry = archive.GetEntry(ManifestEntryName);
            if (manifestEntry == null)
                return null;

            using (var entryStream = manifestEntry.Open())
            using (var reader = new StreamReader(entryStream, Encoding.UTF8))
            {
                return JsonSerializer.Deserialize<SkinManifest>(reader.ReadToEnd());
            }
        }

        private static bool IsZipArchive(string path)
        {
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read))
            {
                if (stream.Length < 4)
                    return false;

                var header = new byte[4];
                stream.ReadExactly(header);
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
                    writer.Write(JsonSerializer.Serialize(manifest, _jsonOptions));
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
            if (image == null || string.IsNullOrEmpty(image.Name))
                return;

            manifestList?.Add(image.Name);

            var entryName = TexturesEntryPrefix + image.Name;
            if (!writtenEntries.Add(entryName))
                return; // same filename already written as part of another group (e.g. shared textures)

            var entry = archive.CreateEntry(entryName, CompressionLevel.Optimal);
            using (var skImage = SKImage.FromBitmap((SKBitmap)image))
            using (var data = skImage.Encode(NamedBitmap.GetImageFormat(image.Name), JpegEncodeQuality))
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
                var manifest = ReadManifest(archive);
                if (manifest == null)
                    return null;

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

        private static ImageGroup ReadGroup(ZipArchive archive, string groupName, List<string> fileNames)
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
            var bitmap = DecodeEntry(archive, fileName);
            return bitmap == null ? null : new NamedBitmap(fileName, bitmap);
        }

        // The compressed bytes go straight into native memory (SKData), sized exactly from the entry, through
        // a buffer small enough to stay off the large object heap: the old copy into a MemoryStream put every
        // texture on the managed LOH, doubled on each growth - tens of MB of garbage per skin that the GC only
        // reclaims on a full collection, and whose committed memory the process then kept.
        // Not SKData.Create(Stream, length): on a stream that can't seek - which a zip entry's is - it
        // returns null, silently, for anything past a few KB (Docs/Internal/avalonia-pitfalls.md, §6).
        private static SKBitmap DecodeEntry(ZipArchive archive, string fileName)
        {
            if (string.IsNullOrEmpty(fileName))
                return null;

            var entry = archive.GetEntry(TexturesEntryPrefix + fileName);
            if (entry == null || entry.Length == 0 || entry.Length > int.MaxValue)
                return null;

            var length = (int)entry.Length;
            using (var entryStream = entry.Open())
            using (var data = SKData.Create(length))
            {
                var buffer = new byte[64 * 1024];
                var filled = 0;
                int read;
                while (filled < length && (read = entryStream.Read(buffer, 0, Math.Min(buffer.Length, length - filled))) > 0)
                {
                    Marshal.Copy(buffer, 0, data.Data + filled, read);
                    filled += read;
                }

                return filled == length ? SKBitmap.Decode(data) : null;
            }
        }

        #endregion
    }
}
