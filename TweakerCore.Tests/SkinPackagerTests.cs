namespace TweakerCore.Tests
{
    using TweakerCore.Engine;
    using NUnit.Framework;
    using SkiaSharp;
    using System;
    using System.IO;

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
