namespace TweakerCore.Tests.Scripting
{
    using System.IO;
    using System.Linq;
    using NUnit.Framework;
    using TweakerCore.PluginInstall;
    using TweakerCore.Scripting;

    /// <summary>
    /// The game's Scripts folder as the Scripts tab lists it, and "restore bundled scripts" - which is
    /// PluginInstallation's table with one row changed, so its tests sit here next to the list they feed.
    /// </summary>
    [TestFixture]
    public class GameScriptsTests
    {
        private ScratchGame _game;

        [SetUp]
        public void SetUp()
        {
            _game = new ScratchGame();
            _game.ShippedScript("puzzlepro_hud.lua", "-- @name PuzzlePro HUD\n-- shipped v1\n")
                 .ShippedScript("puzzlepro_hud.lua.map", "# twllink 1\n");
        }

        [TearDown]
        public void TearDown() => _game.Dispose();

        private GameScript Find(string file) => GameScripts.Scan(_game.Engine, _game.Payload).Single(s => s.FileName == file);

        [Test]
        public void AnInstalledBundleListsItsScriptAsShipped()
        {
            PluginInstallation.Install(_game.Engine, _game.Payload);

            var script = Find("puzzlepro_hud.lua");
            Assert.That(script.Origin, Is.EqualTo(ScriptOrigin.Shipped));
            Assert.That(script.Header.Name, Is.EqualTo("PuzzlePro HUD"));
            Assert.That(script.HasMap, Is.True);
            Assert.That(script.Restorable, Is.False);
            Assert.That(script.IsMissing, Is.False);
            Assert.That(script.Enabled, Is.True);
        }

        [Test]
        public void AnEditedShippedScriptIsModifiedAndRestorable()
        {
            PluginInstallation.Install(_game.Engine, _game.Payload);
            _game.Script("puzzlepro_hud.lua", "-- my edit\n");

            var script = Find("puzzlepro_hud.lua");
            Assert.That(script.Origin, Is.EqualTo(ScriptOrigin.Modified));
            Assert.That(script.Restorable, Is.True);
        }

        [Test]
        public void AScriptTweakerDidNotWriteIsTheUsers()
        {
            _game.Script("mine.lua", "-- @name Mine\n");

            var script = Find("mine.lua");
            Assert.That(script.Origin, Is.EqualTo(ScriptOrigin.User));
            Assert.That(script.Restorable, Is.False);
            Assert.That(script.HasMap, Is.False);
        }

        [Test]
        public void ADeletedShippedScriptIsListedAsMissing()
        {
            PluginInstallation.Install(_game.Engine, _game.Payload);
            File.Delete(_game.ScriptPath("puzzlepro_hud.lua"));

            var script = Find("puzzlepro_hud.lua");
            Assert.That(script.IsMissing, Is.True);
            Assert.That(script.Restorable, Is.True);
            Assert.That(script.Header.Name, Is.EqualTo("PuzzlePro HUD"), "the name comes from the bundle's copy");
        }

        [Test]
        public void OnlyExactlyDotLuaIsAScriptAndTheOrderIsThePlugins()
        {
            _game.Script("b.lua", "").Script("B2.lua", "").Script("a.lua", "").Script("x.LUA", "").Script("y.luac", "")
                 .Script("a.lua.old", "").Script("notes.txt", "");

            var names = GameScripts.Scan(_game.Engine, null).Select(s => s.FileName);
            Assert.That(names, Is.EqualTo(new[] { "B2.lua", "a.lua", "b.lua" }));
        }

        [Test]
        public void BackupsAreListedInTheOrderTheyWereMade()
        {
            _game.Script("a.lua", "").Script("a.lua.old.10", "").Script("a.lua.old", "").Script("a.lua.old.2", "").Script("a.lua.oldish", "");

            Assert.That(Find("a.lua").Backups, Is.EqualTo(new[] { "a.lua.old", "a.lua.old.2", "a.lua.old.10" }));
        }

        [Test]
        public void DisabledComesFromScriptsCfg()
        {
            _game.Script("a.lua", "");
            var config = ScriptsConfig.Load(_game.Engine);
            config.SetEnabled("a.lua", false);
            config.Save();

            Assert.That(Find("a.lua").Enabled, Is.False);
        }

        [Test]
        public void IsShippedChecksTheBundleAndTheManifest()
        {
            Assert.That(GameScripts.IsShipped(_game.Engine, _game.Payload, "puzzlepro_hud.lua"), Is.True);
            Assert.That(GameScripts.IsShipped(_game.Engine, _game.Payload, "mine.lua"), Is.False);

            PluginInstallation.Install(_game.Engine, _game.Payload);
            Assert.That(GameScripts.IsShipped(_game.Engine, null, "puzzlepro_hud.lua"), Is.True, "recorded by an earlier install");
        }

        // --- RestoreScripts -------------------------------------------------------------------------------

        [Test]
        public void RestoreBringsBackADeletedScriptThatInstallWouldNot()
        {
            PluginInstallation.Install(_game.Engine, _game.Payload);
            File.Delete(_game.ScriptPath("puzzlepro_hud.lua"));
            File.Delete(_game.ScriptPath("puzzlepro_hud.lua.map"));

            var report = PluginInstallation.RestoreScripts(_game.Engine, _game.Payload);

            Assert.That(report.Success, Is.True);
            Assert.That(_game.ReadScript("puzzlepro_hud.lua"), Does.Contain("shipped v1"));
            Assert.That(File.Exists(_game.ScriptPath("puzzlepro_hud.lua.map")), Is.True);
        }

        [Test]
        public void RestoreKeepsTheUsersEditAsOld()
        {
            PluginInstallation.Install(_game.Engine, _game.Payload);
            _game.Script("puzzlepro_hud.lua", "-- my edit\n");

            var report = PluginInstallation.RestoreScripts(_game.Engine, _game.Payload);

            Assert.That(report.UpdatedWithBackup.Single().BackupFileName, Is.EqualTo("puzzlepro_hud.lua.old"));
            Assert.That(_game.ReadScript("puzzlepro_hud.lua.old"), Is.EqualTo("-- my edit\n"));
            Assert.That(Find("puzzlepro_hud.lua").Origin, Is.EqualTo(ScriptOrigin.Shipped));
        }

        [Test]
        public void RestoreOfNamedScriptsTakesTheirMapsAndLeavesTheRest()
        {
            _game.ShippedScript("other.lua", "-- other v1\n");
            PluginInstallation.Install(_game.Engine, _game.Payload);
            _game.Script("puzzlepro_hud.lua", "-- edit\n").Script("puzzlepro_hud.lua.map", "edited map").Script("other.lua", "-- other edit\n");

            PluginInstallation.RestoreScripts(_game.Engine, _game.Payload, new[] { "puzzlepro_hud.lua" });

            Assert.That(_game.ReadScript("puzzlepro_hud.lua"), Does.Contain("shipped v1"));
            Assert.That(_game.ReadScript("puzzlepro_hud.lua.map"), Is.EqualTo("# twllink 1\n"));
            Assert.That(_game.ReadScript("other.lua"), Is.EqualTo("-- other edit\n"));
        }

        [Test]
        public void RestoreNeverTouchesThePlugin()
        {
            _game.WritePayload(PluginInstallation.PluginRelativePath, "plugin v2");

            var report = PluginInstallation.RestoreScripts(_game.Engine, _game.Payload);

            Assert.That(report.Success, Is.True);
            Assert.That(PluginInstallation.IsInstalled(_game.Engine), Is.False);
        }

        [Test]
        public void RestoreWithoutBundledScriptsIsAnError()
        {
            Directory.Delete(Path.Combine(_game.Payload, PluginInstallation.StuffFolderName), recursive: true);

            Assert.That(PluginInstallation.RestoreScripts(_game.Engine, _game.Payload).Success, Is.False);
        }
    }
}
