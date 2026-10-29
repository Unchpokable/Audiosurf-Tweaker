namespace TweakerCore.Tests.Skyboxes
{
    using System.IO;
    using System.Linq;
    using NUnit.Framework;
    using SkiaSharp;
    using TweakerCore.PluginInstall;
    using TweakerCore.Skyboxes;
    using TweakerCore.Tests.Scripting;

    /// <summary>The game's Skyboxes folder listed by the plugin catalog's rules, without the plugin.</summary>
    [TestFixture]
    public class GameSkiesTests
    {
        private static string Skyboxes(ScratchGame game)
        {
            var folder = SkyPaths.SkyboxesPath(game.Engine);
            Directory.CreateDirectory(folder);
            return folder;
        }

        private static void Png(string path, int width, int height)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            using var bitmap = new SKBitmap(width, height);
            using var image = SKImage.FromBitmap(bitmap);
            using var data = image.Encode(SKEncodedImageFormat.Png, 100);
            File.WriteAllBytes(path, data.ToArray());
        }

        [Test]
        public void EveryKindIsRecognisedAndSortedLikeTheCatalog()
        {
            using var game = new ScratchGame();
            var folder = Skyboxes(game);

            Directory.CreateDirectory(Path.Combine(folder, "Folder.sky"));
            File.WriteAllText(Path.Combine(folder, "Folder.sky", "Config.json"), "{}");
            File.WriteAllText(Path.Combine(folder, "Zipped.sky"), "a zip, not opened");
            File.WriteAllText(Path.Combine(folder, "Chroma.hlsl"), "");
            Png(Path.Combine(folder, "cross.png"), 400, 300);
            Png(Path.Combine(folder, "pano.png"), 512, 256);
            Png(Path.Combine(folder, "square.png"), 256, 256);
            Png(Path.Combine(folder, "Faces", "px.png"), 128, 128);
            File.WriteAllText(Path.Combine(folder, "notes.txt"), "");
            Directory.CreateDirectory(Path.Combine(folder, "Empty folder"));

            var skies = GameSkies.Scan(game.Engine, game.Payload);

            Assert.That(skies.Select(s => $"{s.Kind}:{s.Id}:{s.DisplayName}:{s.FaceSize}"), Is.EqualTo(new[]
            {
                "ShaderFile:Chroma.hlsl:Chroma:0",
                "File:cross.png:cross:100",
                "File:pano.png:pano:128",
                "FaceDir:Faces:Faces:128",
                "Package:Folder.sky:Folder:0",
                "Package:Zipped.sky:Zipped:0"
            }));
            Assert.That(skies.Single(s => s.Id == "Folder.sky").IsFolder, Is.True);
        }

        [Test]
        public void AManifestWinsOverFacesInTheSameFolder()
        {
            using var game = new ScratchGame();
            var folder = Path.Combine(Skyboxes(game), "Both.sky");
            Png(Path.Combine(folder, "posx.png"), 64, 64);
            File.WriteAllText(Path.Combine(folder, "Config.json"), "{}");

            Assert.That(GameSkies.Scan(game.Engine, game.Payload).Single().Kind, Is.EqualTo(SkyKind.Package));
        }

        [Test]
        public void ARadianceHeaderIsRead()
        {
            using var game = new ScratchGame();
            File.WriteAllText(Path.Combine(Skyboxes(game), "studio.hdr"), "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 512 +X 1024\n");

            Assert.That(GameSkies.Scan(game.Engine, game.Payload).Single().FaceSize, Is.EqualTo(256));
        }

        [Test]
        public void ShippedIsWhatTheBundleHasOrTheInstallationRecorded()
        {
            using var game = new ScratchGame();
            var folder = Skyboxes(game);
            Directory.CreateDirectory(Path.Combine(folder, "Bundled.sky"));
            File.WriteAllText(Path.Combine(folder, "Bundled.sky", "Config.json"), "{}");
            Directory.CreateDirectory(Path.Combine(folder, "Recorded.sky"));
            File.WriteAllText(Path.Combine(folder, "Recorded.sky", "Config.json"), "{}");
            File.WriteAllText(Path.Combine(folder, "Mine.sky"), "");
            game.WritePayload(SkyPaths.SkyboxesRelativeFolder + "Bundled.sky/Config.json", "{}");
            var manifest = new PluginDeployManifest();
            manifest.Set(SkyPaths.SkyboxesRelativeFolder + "Recorded.sky/shaders/a.hlsl", "00");
            manifest.Save(game.Engine);

            var shipped = GameSkies.Scan(game.Engine, game.Payload).ToDictionary(s => s.Id, s => s.IsShipped);

            Assert.That(shipped["Bundled.sky"], Is.True);
            Assert.That(shipped["Recorded.sky"], Is.True);
            Assert.That(shipped["Mine.sky"], Is.False);
            Assert.That(GameSkies.IsShipped(game.Engine, game.Payload, "recorded.SKY"), Is.True, "without case, like the file system");
        }

        [Test]
        public void NoFolderNoSkies()
        {
            using var game = new ScratchGame();

            Assert.That(GameSkies.Scan(game.Engine, game.Payload), Is.Empty);
        }
    }
}
