namespace TweakerCore.Tests.Scripting
{
    using System.IO;
    using NUnit.Framework;
    using TweakerCore.Scripting;

    /// <summary>
    /// scripts.cfg read the way TweakerPlugin/src/lua/lua_config.cxx reads it, and edited without losing what the
    /// plugin or the user put there.
    /// </summary>
    [TestFixture]
    public class ScriptsConfigTests
    {
        private ScratchGame _game;

        [SetUp]
        public void SetUp() => _game = new ScratchGame();

        [TearDown]
        public void TearDown() => _game.Dispose();

        private string ConfigPath => ScriptsConfig.PathFor(_game.Engine);

        private void WriteConfig(string text)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(ConfigPath));
            File.WriteAllText(ConfigPath, text);
        }

        [Test]
        public void NoFileMeansEverythingIsEnabled()
        {
            var config = ScriptsConfig.Load(_game.Engine);

            Assert.That(config.IsEnabled("a.lua"), Is.True);
            Assert.That(config.Disabled, Is.Empty);
        }

        [Test]
        public void ReadsTheValuesThePluginTreatsAsOff()
        {
            WriteConfig("script.a.lua=0\nscript.b.lua = false\n  script.c.lua=off  \nscript.d.lua=1\nscript.e.lua=no\n# script.f.lua=0\nscript.=0\n");
            var config = ScriptsConfig.Load(_game.Engine);

            Assert.That(config.Disabled, Is.EqualTo(new[] { "a.lua", "b.lua", "c.lua" }));
            Assert.That(config.IsEnabled("d.lua"), Is.True);
            Assert.That(config.IsEnabled("e.lua"), Is.True);
            Assert.That(config.IsEnabled("f.lua"), Is.True);
        }

        [Test]
        public void NamesCompareCaseSensitivelyLikeThePlugin()
        {
            WriteConfig("script.Demo.lua=0\n");

            Assert.That(ScriptsConfig.Load(_game.Engine).IsEnabled("demo.lua"), Is.True);
        }

        [Test]
        public void DisablingIntoAMissingFileWritesThePluginsShape()
        {
            var config = ScriptsConfig.Load(_game.Engine);
            config.SetEnabled("a.lua", false);
            config.Save();

            var lines = File.ReadAllLines(ConfigPath);
            Assert.That(lines[0], Does.StartWith("# Scripts the overlay should not run."));
            Assert.That(lines[^1], Is.EqualTo("script.a.lua=0"));
            Assert.That(ScriptsConfig.Load(_game.Engine).IsEnabled("a.lua"), Is.False);
        }

        [Test]
        public void EnablingRemovesEveryLineOfThatScriptAndNothingElse()
        {
            WriteConfig("# mine\nscript.a.lua=0\nscript.b.lua=0\nscript.a.lua=off\n\nengine.settle_ms=900\n");
            var config = ScriptsConfig.Load(_game.Engine);
            config.SetEnabled("a.lua", true);
            config.Save();

            Assert.That(File.ReadAllText(ConfigPath), Is.EqualTo("# mine\nscript.b.lua=0\n\nengine.settle_ms=900\n"));
        }

        [Test]
        public void DisablingGoesAfterTheOtherScriptsAndAboveTheEngineBlock()
        {
            WriteConfig("# header\nscript.a.lua=0\n\n# tuning\nengine.settle_frames=60\n");
            var config = ScriptsConfig.Load(_game.Engine);
            config.SetEnabled("b.lua", false);
            config.Save();

            Assert.That(File.ReadAllText(ConfigPath), Is.EqualTo("# header\nscript.a.lua=0\nscript.b.lua=0\n\n# tuning\nengine.settle_frames=60\n"));
        }

        [Test]
        public void DisablingTwiceWritesOneLine()
        {
            var config = ScriptsConfig.Load(_game.Engine);
            config.SetEnabled("a.lua", false);
            config.SetEnabled("a.lua", false);

            Assert.That(config.Disabled, Is.EqualTo(new[] { "a.lua" }));
        }
    }
}
