namespace TweakerCore.Tests.Skyboxes
{
    using System.IO;
    using NUnit.Framework;
    using TweakerCore.Skyboxes;
    using TweakerCore.Tests.Scripting;

    /// <summary>module.json read the way the plugin reads it, and written by changing only "enabled" and "sky".</summary>
    [TestFixture]
    public class SkyModuleConfigTests
    {
        private const string PluginsOwn =
            "{\n"
            + "  \"enabled\": true,\n"
            + "  \"sky\": { \"kind\": \"package\", \"id\": \"ODC_VolTex.sky\" },\n"
            + "  \"cubemap\": { \"hdr_exposure\": 1.5, \"min_face_size\": 0 },\n"
            + "  \"orientation\": { \"yaw_degrees\": 12, \"pitch_degrees\": 0, \"z_up\": false },\n"
            + "  \"shader\": { \"quality\": 67 }\n"
            + "}\n";

        private static string PathOf(ScratchGame game) => SkyPaths.ModuleConfigPath(game.Engine);

        private static void Write(ScratchGame game, string text)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(PathOf(game)));
            File.WriteAllText(PathOf(game), text);
        }

        [Test]
        public void AMissingFileMeansTheDefaults()
        {
            using var game = new ScratchGame();

            var config = SkyModuleConfig.Load(game.Engine);

            Assert.That(config.Exists, Is.False);
            Assert.That(config.Enabled, Is.True);
            Assert.That(config.Sky, Is.EqualTo(SkyModuleConfig.DefaultSky));
        }

        [Test]
        public void ThePluginsFileReadsAsItsSelection()
        {
            using var game = new ScratchGame();
            Write(game, PluginsOwn.Replace("\"enabled\": true", "\"enabled\": false"));

            var config = SkyModuleConfig.Load(game.Engine);

            Assert.That(config.Problem, Is.Null);
            Assert.That(config.Enabled, Is.False);
            Assert.That(config.Sky, Is.EqualTo(new SkySelection(SkyKind.Package, "ODC_VolTex.sky")));
        }

        [TestCase("{ \"sky\": { \"kind\": \"skydome\", \"id\": \"x\" } }")]
        [TestCase("{ \"sky\": { \"kind\": \"package\", \"id\": \"\" } }")]
        [TestCase("{ \"sky\": { \"kind\": \"package\" } }")]
        [TestCase("{ \"sky\": \"ODC_VolTex.sky\" }")]
        public void ASelectionThePluginCannotMeanIsTheDefault(string text)
        {
            using var game = new ScratchGame();
            Write(game, text);

            Assert.That(SkyModuleConfig.Load(game.Engine).Sky, Is.EqualTo(SkyModuleConfig.DefaultSky));
        }

        [Test]
        public void WritingChangesTheTwoFieldsAndNothingElse()
        {
            using var game = new ScratchGame();
            Write(game, PluginsOwn.Replace("{\n", "{\n  // mine\n"));

            SkyModuleConfig.Write(game.Engine, false, new SkySelection(SkyKind.Package, "My Sky.sky"));

            Assert.That(File.ReadAllText(PathOf(game)), Is.EqualTo(PluginsOwn
                .Replace("{\n", "{\n  // mine\n")
                .Replace("\"enabled\": true", "\"enabled\": false")
                .Replace("ODC_VolTex.sky", "My Sky.sky")));

            SkyModuleConfig.Write(game.Engine, true, null);
            var config = SkyModuleConfig.Load(game.Engine);
            Assert.That(config.Enabled, Is.True);
            Assert.That(config.Sky.Id, Is.EqualTo("My Sky.sky"), "null leaves the selection alone");
        }

        [Test]
        public void AMissingFileIsCreatedWithJustTheTwo()
        {
            using var game = new ScratchGame();

            SkyModuleConfig.Write(game.Engine, null, new SkySelection(SkyKind.Program, "night"));

            Assert.That(File.ReadAllText(PathOf(game)), Is.EqualTo("{\n  \"enabled\": true,\n  \"sky\": { \"kind\": \"program\", \"id\": \"night\" }\n}\n"));
        }

        [Test]
        public void ABrokenFileIsNeverWritten()
        {
            using var game = new ScratchGame();
            Write(game, "{ \"enabled\": tru");

            var config = SkyModuleConfig.Load(game.Engine);
            Assert.That(config.Problem, Does.Contain("not valid JSON"));
            Assert.Throws<InvalidDataException>(() => SkyModuleConfig.Write(game.Engine, false, null));
            Assert.That(File.ReadAllText(PathOf(game)), Is.EqualTo("{ \"enabled\": tru"));
        }

        [Test]
        public void ADiskSelectionMatchesWithoutCaseABuiltInOneWithIt()
        {
            Assert.That(new SkySelection(SkyKind.Package, "My Sky.sky").Matches(SkyKind.Package, "my sky.SKY"), Is.True);
            Assert.That(new SkySelection(SkyKind.Program, "night").Matches(SkyKind.Program, "Night"), Is.False);
            Assert.That(new SkySelection(SkyKind.Package, "a").Matches(SkyKind.File, "a"), Is.False);
        }
    }
}
