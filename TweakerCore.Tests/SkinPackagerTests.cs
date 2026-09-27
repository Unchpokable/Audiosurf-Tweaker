namespace TweakerCore.Tests
{
    using TweakerCore.Engine;
    using NUnit.Framework;
    using SkiaSharp;
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.IO.Compression;
    using System.Linq;
    using System.Text.Json;

    [TestFixture]
    public class SkinPackagerTests
    {
        private string _tempDir;

        [SetUp]
        public void SetUp()
        {
            _tempDir = Path.Combine(Path.GetTempPath(), "TweakerCoreTests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(_tempDir);
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(_tempDir))
                Directory.Delete(_tempDir, recursive: true);
        }

        [Test]
        public void CompileDecompile_RoundTrip_PreservesData()
        {
            var path = Path.Combine(_tempDir, "roundtrip.tasp");
            Exception captured = null;
            Action<string, Exception> handler = (msg, ex) => captured = ex;
            SkinPackager.OperationFailed += handler;

            try
            {
                using (var skin = BuildSampleSkin("Round Trip Skin"))
                {
                    var ok = SkinPackager.CompileToFile(skin, path);
                    if (!ok)
                        Assert.Fail($"Compile failed: {captured}");
                }
            }
            finally
            {
                SkinPackager.OperationFailed -= handler;
            }

            Assert.IsTrue(File.Exists(path));

            using (var loaded = SkinPackager.Decompile(path))
            {
                Assert.IsNotNull(loaded);
                Assert.AreEqual("Round Trip Skin", loaded.Name);
                Assert.AreEqual(1, loaded.Cliffs.Group.Count);
                Assert.AreEqual("cliff-1.png", loaded.Cliffs.Group[0].Name);
                Assert.IsNotNull(loaded.Tiles);
                Assert.AreEqual("tiles.png", loaded.Tiles.Name);
                Assert.IsNotNull(loaded.Cover);
                Assert.AreEqual("cover.png", loaded.Cover.Name);

                var bitmap = (SKBitmap)loaded.Tiles;
                Assert.AreEqual(2, bitmap.Width);
                Assert.AreEqual(2, bitmap.Height);
            }
        }

        [Test]
        public void RenameSkin_RewritesOnlyManifest()
        {
            var oldPath = Path.Combine(_tempDir, "Old Name.tasp");
            var newPath = Path.Combine(_tempDir, "New Name.tasp");
            using (var skin = BuildSampleSkin("Old Name"))
                Assert.IsTrue(SkinPackager.CompileToFile(skin, oldPath));

            var texturesBefore = ReadTextureEntries(oldPath);
            var uidBefore = ReadManifest(oldPath).GetProperty("Uid").GetString();

            Assert.IsTrue(SkinPackager.RenameSkin(oldPath, "New Name", newPath));

            Assert.IsFalse(File.Exists(oldPath));
            Assert.IsTrue(File.Exists(newPath));
            Assert.IsFalse(File.Exists(newPath + ".renaming"));

            var manifest = ReadManifest(newPath);
            Assert.AreEqual("New Name", manifest.GetProperty("Name").GetString());
            Assert.AreEqual(uidBefore, manifest.GetProperty("Uid").GetString());
            Assert.That(ReadTextureEntries(newPath).Keys, Is.EquivalentTo(texturesBefore.Keys));
            foreach (var (entry, bytes) in ReadTextureEntries(newPath))
                Assert.That(bytes, Is.EqualTo(texturesBefore[entry]), entry);
        }

        [Test]
        public void RenameSkin_SamePath_KeepsFileAndChangesName()
        {
            var path = Path.Combine(_tempDir, "file-name.tasp");
            using (var skin = BuildSampleSkin("Old Name"))
                Assert.IsTrue(SkinPackager.CompileToFile(skin, path));

            Assert.IsTrue(SkinPackager.RenameSkin(path, "New Name", path));

            Assert.IsTrue(File.Exists(path));
            Assert.AreEqual("New Name", ReadManifest(path).GetProperty("Name").GetString());
        }

        [Test]
        public void RenameSkin_OldFileLocked_LeavesOriginalOnly()
        {
            var oldPath = Path.Combine(_tempDir, "Old Name.tasp");
            var newPath = Path.Combine(_tempDir, "New Name.tasp");
            using (var skin = BuildSampleSkin("Old Name"))
                Assert.IsTrue(SkinPackager.CompileToFile(skin, oldPath));

            using (new FileStream(oldPath, FileMode.Open, FileAccess.Read, FileShare.Read))
                Assert.IsFalse(SkinPackager.RenameSkin(oldPath, "New Name", newPath));

            Assert.IsTrue(File.Exists(oldPath));
            Assert.IsFalse(File.Exists(newPath));
            Assert.IsFalse(File.Exists(newPath + ".renaming"));
            Assert.AreEqual("Old Name", ReadManifest(oldPath).GetProperty("Name").GetString());
        }

        private static JsonElement ReadManifest(string path)
        {
            using var archive = ZipFile.OpenRead(path);
            using var stream = archive.GetEntry("manifest.json").Open();
            return JsonDocument.Parse(stream).RootElement.Clone();
        }

        private static Dictionary<string, byte[]> ReadTextureEntries(string path)
        {
            using var archive = ZipFile.OpenRead(path);
            var result = new Dictionary<string, byte[]>();
            foreach (var entry in archive.Entries.Where(e => e.FullName.StartsWith("textures/")))
            {
                using var stream = entry.Open();
                using var memory = new MemoryStream();
                stream.CopyTo(memory);
                result[entry.FullName] = memory.ToArray();
            }
            return result;
        }

        [Test]
        public void Decompile_NonSkinFile_ReturnsNull()
        {
            var path = Path.Combine(_tempDir, "notaskin.txt");
            File.WriteAllText(path, "definitely not a skin");

            Assert.IsNull(SkinPackager.Decompile(path));
        }

        [Test]
        public void Decompile_MissingFile_ReturnsNull()
        {
            Assert.IsNull(SkinPackager.Decompile(Path.Combine(_tempDir, "missing.tasp")));
        }

        [Test]
        public void ReadName_ReturnsManifestName()
        {
            var path = Path.Combine(_tempDir, "file name differs.tasp");
            using (var skin = BuildSampleSkin("Name In Manifest"))
                Assert.IsTrue(SkinPackager.CompileToFile(skin, path));

            Assert.AreEqual("Name In Manifest", SkinPackager.ReadName(path));
        }

        [Test]
        public void ReadName_NonSkinOrMissingFile_ReturnsNull()
        {
            var path = Path.Combine(_tempDir, "notaskin.txt");
            File.WriteAllText(path, "definitely not a skin");

            Assert.IsNull(SkinPackager.ReadName(path));
            Assert.IsNull(SkinPackager.ReadName(Path.Combine(_tempDir, "missing.tasp")));
        }

        [Test]
        public void ReadPreviews_DecodesOnlyPreviewsInManifestOrder()
        {
            var path = Path.Combine(_tempDir, "previews.tasp");
            using (var skin = BuildSampleSkin("Previews"))
            {
                skin.SkySpheres.AddImage(new NamedBitmap("Skysphere_Grey.png", new SKBitmap(64, 32)));
                // Noise, so the entries are real-sized: a zip entry stream that isn't seekable only shows
                // its problems (SKData.Create(Stream, length) returned null on it) past a few KB.
                skin.Previews.AddImage(new NamedBitmap("Screenshot1.png", CreateNoiseBitmap(640, 360)));
                skin.Previews.AddImage(new NamedBitmap("Screenshot2.jpg", CreateNoiseBitmap(320, 180)));
                Assert.IsTrue(SkinPackager.CompileToFile(skin, path));
            }

            var seen = new List<SKBitmap>();
            var sizes = SkinPackager.ReadPreviews(path, bitmap =>
            {
                seen.Add(bitmap);
                return (bitmap.Width, bitmap.Height);
            });

            Assert.That(sizes, Is.EqualTo(new[] { (640, 360), (320, 180) }));
            // Handed over one at a time and disposed by the packager, not by the caller.
            Assert.That(seen.Select(b => b.Handle), Is.All.EqualTo(IntPtr.Zero));
        }

        [Test]
        public void ReadPreviews_SkinWithoutPreviews_ReturnsEmpty()
        {
            var path = Path.Combine(_tempDir, "nopreviews.tasp");
            using (var skin = BuildSampleSkin("No Previews"))
                Assert.IsTrue(SkinPackager.CompileToFile(skin, path));

            Assert.That(SkinPackager.ReadPreviews(path, bitmap => bitmap.Width), Is.Empty);
        }

        [Test]
        public void DeepClone_ProducesIndependentBitmapCopies()
        {
            using (var original = BuildSampleSkin("Original"))
            using (var clone = original.DeepClone())
            {
                Assert.AreEqual(original.Name, clone.Name);
                Assert.AreNotSame(original.Tiles, clone.Tiles);

                var originalBitmap = (SKBitmap)original.Tiles;
                var cloneBitmap = (SKBitmap)clone.Tiles;

                Assert.AreNotSame(originalBitmap, cloneBitmap);

                cloneBitmap.SetPixel(0, 0, SKColors.Black);
                Assert.AreNotEqual(cloneBitmap.GetPixel(0, 0), originalBitmap.GetPixel(0, 0));
            }
        }

        private static AudiosurfSkinExtended BuildSampleSkin(string name)
        {
            var skin = new AudiosurfSkinExtended { Name = name };
            skin.Cliffs.AddImage(new NamedBitmap("cliff-1.png", CreateTestBitmap(SKColors.Red)));
            skin.Tiles = new NamedBitmap("tiles.png", CreateTestBitmap(SKColors.Blue));
            skin.TilesFlyup = new NamedBitmap("tileflyup.png", CreateTestBitmap(SKColors.Green));
            skin.Cover = new NamedBitmap("cover.png", CreateTestBitmap(SKColors.Yellow));
            return skin;
        }

        private static SKBitmap CreateNoiseBitmap(int width, int height)
        {
            var random = new Random(width * 31 + height);
            var bitmap = new SKBitmap(width, height);
            for (var y = 0; y < height; y++)
                for (var x = 0; x < width; x++)
                    bitmap.SetPixel(x, y, new SKColor((byte)random.Next(256), (byte)random.Next(256), (byte)random.Next(256)));
            return bitmap;
        }

        private static SKBitmap CreateTestBitmap(SKColor color)
        {
            var bitmap = new SKBitmap(2, 2);
            using (var canvas = new SKCanvas(bitmap))
            {
                canvas.Clear(color);
            }
            return bitmap;
        }
    }
}
