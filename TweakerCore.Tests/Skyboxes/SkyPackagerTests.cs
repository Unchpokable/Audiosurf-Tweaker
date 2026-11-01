namespace TweakerCore.Tests.Skyboxes
{
    using System;
    using System.IO;
    using System.IO.Compression;
    using System.Linq;
    using NUnit.Framework;
    using TweakerCore.PluginInstall;
    using TweakerCore.Skyboxes;
    using TweakerCore.Tests.Scripting;

    /// <summary>
    /// The check before packing (only what it promises: the manifest parses, has layers, and every file it names is in
    /// the package and is a name the plugin reads) and the install into a fake game - atomic, refusing what is not ours
    /// to move.
    /// </summary>
    [TestFixture]
    public class SkyPackagerTests
    {
        private string _root;
        private string _project;

        [SetUp]
        public void SetUp()
        {
            _root = Path.Combine(Path.GetTempPath(), "TweakerCoreTests", "SkyPackager", Guid.NewGuid().ToString("N"));
            _project = Path.Combine(_root, "project");
            SkyTemplates.Create("gradient", _project, "My Sky", "");
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }

        private void Manifest(string json) => File.WriteAllText(Path.Combine(_project, "Config.json"), json);

        private string[] Errors() => SkyPackager.Check(_project).Where(p => p.IsError).Select(p => p.Message).ToArray();

        private static string Layers(string layers) => "{ \"format\": 1, \"name\": \"My Sky\", \"layers\": [" + layers + "] }";

        [Test]
        public void AMissingShaderIsAnErrorNamingIt()
        {
            Manifest(Layers("{ \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \"shaders/nope.hlsl\" }"));

            Assert.That(Errors(), Is.EqualTo(new[] { "layer 'sky': shader 'shaders/nope.hlsl' is not in the project" }));
        }

        [Test]
        public void NamesAreComparedWithoutCaseAndEitherSlash()
        {
            Manifest(Layers("{ \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \"./SHADERS\\\\Sky.HLSL\" }"));

            Assert.That(Errors(), Is.Empty);
        }

        [TestCase("../outside.hlsl", "leaves the package")]
        [TestCase("C:/sky.hlsl", "is not a path inside the package")]
        [TestCase("shaders/sky.hlsl:stream", "has a character a file name cannot have")]
        [TestCase("shaders/con.hlsl", "names a device")]
        public void ANameThePluginWouldRefuseIsAnError(string path, string why)
        {
            Manifest(Layers("{ \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \"" + path + "\" }"));

            Assert.That(Errors().Single(), Does.Contain(why));
        }

        [Test]
        public void AFileThatIsLeftOutOfThePackageCountsAsMissing()
        {
            Directory.CreateDirectory(Path.Combine(_project, ".work"));
            File.WriteAllText(Path.Combine(_project, ".work", "sky.hlsl"), "");
            Manifest(Layers("{ \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \".work/sky.hlsl\" }"));

            Assert.That(Errors().Single(), Does.Contain("is not packed"));
        }

        [Test]
        public void ASpriteLayerNeedsBothHalvesOfItsPairOrNoneAtAll()
        {
            File.WriteAllText(Path.Combine(_project, "shaders", "clouds.ps.hlsl"), "");
            Manifest(Layers("{ \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \"shaders/sky.hlsl\" },"
                            + "{ \"id\": \"clouds\", \"kind\": \"sprites\", \"shader\": \"shaders/clouds\" },"
                            + "{ \"id\": \"builtin\", \"kind\": \"sprites\" }"));

            Assert.That(Errors(), Is.EqualTo(new[] { "layer 'clouds': vertex shader 'shaders/clouds.vs.hlsl' is not in the project" }));
        }

        [Test]
        public void FillsGeneratorsAndTexturesAreChecked()
        {
            Manifest(Layers("{ \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \"shaders/sky.hlsl\", \"textures\": [ { \"path\": \"textures/noise.png\", \"slot\": 0 } ] },"
                            + "{ \"id\": \"a\", \"kind\": \"sprites\", \"generator\": \"scripts/place.lua\", \"fill\": { \"kind\": \"atlas\" } },"
                            + "{ \"id\": \"b\", \"kind\": \"sprites\", \"fill\": { \"kind\": \"texture\", \"path\": \"textures/sheet.png\" } },"
                            + "{ \"id\": \"c\", \"kind\": \"sprites\", \"fill\": { \"kind\": \"shader\" } },"
                            + "{ \"id\": \"d\", \"kind\": \"sprites\", \"fill\": { \"kind\": \"paint\" } }"));

            Assert.That(Errors(), Is.EquivalentTo(new[]
            {
                "layer 'sky': texture 'textures/noise.png' - layer textures are .dds only",
                "layer 'sky': texture 'textures/noise.png' is not in the project",
                "layer 'a': generator 'scripts/place.lua' is not in the project",
                "layer 'a': fill kind 'atlas' needs a \"generator\" to bake the tiles",
                "layer 'b': fill texture 'textures/sheet.png' is not in the project",
                "layer 'c': fill kind 'shader' needs the layer to name its own \"shader\" - the built-in one samples an atlas",
                "layer 'd': unknown fill kind 'paint' - builtin, atlas, texture or shader"
            }));
        }

        [TestCase("{ \"name\": \"My Sky\" }", "no \"layers\" - the sky has nothing to draw")]
        [TestCase("{ \"name\": \"My Sky\", \"layers\": [ { \"id\": \"x\", \"kind\": \"skydome\" } ] }", "layer 'x': unknown \"kind\" 'skydome' - fullsky or sprites")]
        [TestCase("[ 1 ]", "the top level must be an object")]
        [TestCase("{ \"name\": \"My/Sky\", \"layers\": [ { \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \"shaders/sky.hlsl\" } ] }", "\"name\": It is a file name")]
        public void AManifestWithNothingToDrawOrABadNameIsAnError(string json, string expected)
        {
            Manifest(json);

            Assert.That(Errors(), Has.Some.StartsWith(expected));
        }

        [Test]
        public void ANewerFormatIsRefusedAndTheInstalledPackageIsUntouched()
        {
            using var game = new ScratchGame();
            var first = SkyPackager.Install(_project, game.Engine);
            var before = File.ReadAllBytes(first.Path);
            Manifest("{ \"format\": 2, \"name\": \"My Sky\", \"layers\": ["
                     + "{ \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \"shaders/sky.hlsl\" } ] }");

            var result = SkyPackager.Install(_project, game.Engine);
            Assert.That(result.Outcome, Is.EqualTo(SkyInstallOutcome.Problems));
            Assert.That(result.Problems.Any(p => p.IsError && p.Message.Contains("format")), Is.True);
            Assert.That(File.ReadAllBytes(first.Path), Is.EqualTo(before));
        }

        [Test]
        public void TwinLayerIdsAreWarningsOnly()
        {
            Manifest("{ \"format\": 1, \"name\": \"My Sky\", \"layers\": ["
                     + "{ \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \"shaders/sky.hlsl\" },"
                     + "{ \"id\": \"sky\", \"kind\": \"fullsky\", \"shader\": \"shaders/sky.hlsl\" } ] }");

            var problems = SkyPackager.Check(_project);
            Assert.That(problems.All(p => !p.IsError), Is.True);
            Assert.That(problems.Count, Is.EqualTo(1));
        }

        [Test]
        public void InstallingWritesTheZipIntoTheGameAndReplacesTheLastOne()
        {
            using var game = new ScratchGame();

            var first = SkyPackager.Install(_project, game.Engine);
            Assert.That(first.Outcome, Is.EqualTo(SkyInstallOutcome.Installed), first.Message);
            Assert.That(first.Path, Is.EqualTo(Path.Combine(SkyPaths.SkyboxesPath(game.Engine), "My Sky.sky")));
            Assert.That(first.FileName, Is.EqualTo("My Sky.sky"));

            File.AppendAllText(Path.Combine(_project, "shaders", "sky.hlsl"), "\n// edited\n");
            var second = SkyPackager.Install(_project, game.Engine);
            Assert.That(second.Succeeded, Is.True, second.Message);

            using (var archive = ZipFile.OpenRead(second.Path))
            {
                Assert.That(archive.GetEntry("Config.json"), Is.Not.Null, "Config.json at the top of the archive");
                using var reader = new StreamReader(archive.GetEntry("shaders/sky.hlsl").Open());
                Assert.That(reader.ReadToEnd(), Does.EndWith("// edited\n"));
                Assert.That(archive.Entries.Any(e => e.FullName.StartsWith(".tweaker")), Is.False);
            }
            Assert.That(Directory.GetFileSystemEntries(SkyPaths.SkyboxesPath(game.Engine)).Select(Path.GetFileName), Is.EqualTo(new[] { "My Sky.sky" }),
                "the temporary file is gone");
        }

        [Test]
        public void AProjectWithErrorsIsNotInstalledAndTheGameKeepsWhatItHad()
        {
            using var game = new ScratchGame();
            SkyPackager.Install(_project, game.Engine);
            var installed = Path.Combine(SkyPaths.SkyboxesPath(game.Engine), "My Sky.sky");
            var before = File.ReadAllBytes(installed);

            File.Delete(Path.Combine(_project, "shaders", "sky.hlsl"));
            var result = SkyPackager.Install(_project, game.Engine);

            Assert.That(result.Outcome, Is.EqualTo(SkyInstallOutcome.Problems));
            Assert.That(File.ReadAllBytes(installed), Is.EqualTo(before));
        }

        [Test]
        public void AFolderOfTheSameNameIsNotOursToMove()
        {
            using var game = new ScratchGame();
            var folder = Path.Combine(SkyPaths.SkyboxesPath(game.Engine), "My Sky.sky");
            Directory.CreateDirectory(folder);
            File.WriteAllText(Path.Combine(folder, "Config.json"), "{}");

            var result = SkyPackager.Install(_project, game.Engine);

            Assert.That(result.Outcome, Is.EqualTo(SkyInstallOutcome.Refused));
            Assert.That(result.Message, Does.Contain("folder called My Sky.sky"));
            Assert.That(File.ReadAllText(Path.Combine(folder, "Config.json")), Is.EqualTo("{}"));
        }

        [Test]
        public void AGameWithoutThePluginIsRefused()
        {
            var engine = Path.Combine(_root, "Audiosurf", "engine");
            Directory.CreateDirectory(Path.Combine(engine, PluginInstallation.ChannelsFolderName));

            var result = SkyPackager.Install(_project, engine);

            Assert.That(result.Outcome, Is.EqualTo(SkyInstallOutcome.Refused));
            Assert.That(Directory.Exists(Path.Combine(engine, PluginInstallation.StuffFolderName)), Is.False);
        }

        [Test]
        public void ExportWritesTheSamePackageAnywhere()
        {
            var target = Path.Combine(_root, "share", "My Sky.sky");

            var result = SkyPackager.Export(_project, target);

            Assert.That(result.Succeeded, Is.True, result.Message);
            using var archive = ZipFile.OpenRead(target);
            Assert.That(archive.Entries.Select(e => e.FullName), Is.EquivalentTo(SkyPackager.PackageFiles(_project)));
        }
    }
}
