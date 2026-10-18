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
        public void ReadsTheOrderThePluginWrote()
        {
            WriteConfig("script.b.lua=0\n\n# The order scripts run in\norder=c.lua\n order = a.lua \norder=\norder=c.lua\n# order=z.lua\norderx=y.lua\n");

            Assert.That(ScriptsConfig.Load(_game.Engine).Order, Is.EqualTo(new[] { "c.lua", "a.lua" }));
        }

        [Test]
        public void NoOrderLinesIsAnEmptyOrder()
        {
            WriteConfig("script.a.lua=0\n");

            Assert.That(ScriptsConfig.Load(_game.Engine).Order, Is.Empty);
        }

        [Test]
        public void SettingTheOrderReplacesItsLinesWhereTheyWere()
        {
            WriteConfig("# header\nscript.a.lua=0\n\n# The order\norder=a.lua\norder=b.lua\n\nengine.settle_ms=900\n");
            var config = ScriptsConfig.Load(_game.Engine);
            config.SetOrder(new[] { "b.lua", "a.lua", "c.lua" });
            config.Save();

            Assert.That(File.ReadAllText(ConfigPath),
                Is.EqualTo("# header\nscript.a.lua=0\n\n# The order\norder=b.lua\norder=a.lua\norder=c.lua\n\nengine.settle_ms=900\n"));
        }

        [Test]
        public void TheFirstOrderGoesAtTheEndWithThePluginsComment()
        {
            WriteConfig("script.a.lua=0\n");
            var config = ScriptsConfig.Load(_game.Engine);
            config.SetOrder(new[] { "b.lua", "a.lua" });
            config.Save();

            Assert.That(File.ReadAllText(ConfigPath),
                Is.EqualTo("script.a.lua=0\n\n# The order scripts run in, top first. Set by dragging rows in the Scripts tab;\n"
                           + "# scripts not listed run after these, by file name.\norder=b.lua\norder=a.lua\n"));
            Assert.That(ScriptsConfig.Load(_game.Engine).Order, Is.EqualTo(new[] { "b.lua", "a.lua" }));
        }

        [Test]
        public void DisablingAfterAnOrderStaysWithTheScriptLines()
        {
            WriteConfig("script.a.lua=0\n\norder=b.lua\norder=a.lua\n");
            var config = ScriptsConfig.Load(_game.Engine);
            config.SetEnabled("b.lua", false);
            config.Save();

            Assert.That(File.ReadAllText(ConfigPath), Is.EqualTo("script.a.lua=0\nscript.b.lua=0\n\norder=b.lua\norder=a.lua\n"));
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
