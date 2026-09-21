namespace TweakerCore.Tests
{
    using NUnit.Framework;
    using System;
    using System.IO;
    using System.Linq;
    using TweakerCore.PluginInstall;

    /// <summary>
    /// The table in Docs/Internal/plugin-offline-mode.md §6.3, one test per row, plus the rules around it:
    /// files dropped from the bundle, repeat installs, uninstall, and what happens without a usable manifest.
    ///
    /// No real DLL takes part: the "plugin" here is a text file, so FileVersionInfo finds no version and the
    /// comparison falls through to the hash - which is the path a same-version rebuild takes anyway.
    /// </summary>
    [TestFixture]
    public class PluginInstallationTests
    {
        private const string ScriptPath = "TweakerStuff/Scripts/puzzlepro_hud.lua";

        private string _root;
        private string _engine;
        private string _payload;

        [SetUp]
        public void SetUp()
        {
            _root = Path.Combine(Path.GetTempPath(), "TweakerCoreTests", "PluginInstall", Guid.NewGuid().ToString("N"));
            _engine = Path.Combine(_root, "engine");
            _payload = Path.Combine(_root, "PluginPayload");

            // A folder only counts as the engine folder with both of these in it.
            Directory.CreateDirectory(Path.Combine(_engine, PluginInstallation.ChannelsFolderName));
            File.WriteAllText(Path.Combine(_engine, PluginInstallation.GameExecutableName), "not really the game");

            WritePayload(PluginInstallation.PluginRelativePath, "plugin v1");
            WritePayload(ScriptPath, "-- shipped script v1");
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }

        [Test]
        public void Install_FreshGame_CopiesEverythingAndRecordsIt()
        {
            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Success, Is.True);
            Assert.That(report.Added, Is.EquivalentTo(new[] { PluginInstallation.PluginRelativePath, ScriptPath }));
            Assert.That(InstalledText(PluginInstallation.PluginRelativePath), Is.EqualTo("plugin v1"));
            Assert.That(InstalledText(ScriptPath), Is.EqualTo("-- shipped script v1"));
            Assert.That(PluginInstallation.IsInstalled(_engine), Is.True);

            var manifest = PluginDeployManifest.Load(_engine);
            Assert.That(manifest.Exists, Is.True);
            Assert.That(manifest.Files.Keys, Is.EquivalentTo(new[] { PluginInstallation.PluginRelativePath, ScriptPath }));
        }

        [Test]
        public void Install_ShippedFileUntouchedByUser_IsOverwrittenSilently()
        {
            PluginInstallation.Install(_engine, _payload);
            WritePayload(ScriptPath, "-- shipped script v2");

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Success, Is.True);
            Assert.That(report.Updated, Does.Contain(ScriptPath));
            Assert.That(report.UpdatedWithBackup, Is.Empty);
            Assert.That(InstalledText(ScriptPath), Is.EqualTo("-- shipped script v2"));
            Assert.That(BackupsOf(ScriptPath), Is.Empty);
        }

        [Test]
        public void Install_ShippedFileEditedByUser_KeepsTheirVersionAsOld()
        {
            PluginInstallation.Install(_engine, _payload);
            WriteInstalled(ScriptPath, "-- my own tweaks");
            WritePayload(ScriptPath, "-- shipped script v2");

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Success, Is.True);
            Assert.That(report.Updated, Is.Empty);
            Assert.That(report.UpdatedWithBackup.Count, Is.EqualTo(1));
            Assert.That(report.UpdatedWithBackup[0].RelativePath, Is.EqualTo(ScriptPath));
            Assert.That(report.UpdatedWithBackup[0].BackupFileName, Is.EqualTo("puzzlepro_hud.lua.old"));

            Assert.That(InstalledText(ScriptPath), Is.EqualTo("-- shipped script v2"));
            Assert.That(File.ReadAllText(InstalledPath(ScriptPath) + ".old"), Is.EqualTo("-- my own tweaks"));
        }

        [Test]
        public void Install_BackupNameAlreadyTaken_NumbersTheNextOne()
        {
            PluginInstallation.Install(_engine, _payload);
            WriteInstalled(ScriptPath, "-- edit one");
            WritePayload(ScriptPath, "-- shipped script v2");
            PluginInstallation.Install(_engine, _payload);

            WriteInstalled(ScriptPath, "-- edit two");
            WritePayload(ScriptPath, "-- shipped script v3");
            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.UpdatedWithBackup[0].BackupFileName, Is.EqualTo("puzzlepro_hud.lua.old.2"));
            Assert.That(File.ReadAllText(InstalledPath(ScriptPath) + ".old"), Is.EqualTo("-- edit one"));
            Assert.That(File.ReadAllText(InstalledPath(ScriptPath) + ".old.2"), Is.EqualTo("-- edit two"));
            Assert.That(InstalledText(ScriptPath), Is.EqualTo("-- shipped script v3"));
        }

        [Test]
        public void Install_ShippedFileDeletedByUser_IsNotBroughtBack()
        {
            PluginInstallation.Install(_engine, _payload);
            File.Delete(InstalledPath(ScriptPath));
            WritePayload(ScriptPath, "-- shipped script v2");

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.SkippedDeleted, Does.Contain(ScriptPath));
            Assert.That(File.Exists(InstalledPath(ScriptPath)), Is.False);

            // Still remembered, so the next update leaves it alone as well rather than treating it as new.
            var manifest = PluginDeployManifest.Load(_engine);
            Assert.That(manifest.Files.ContainsKey(ScriptPath), Is.True);
        }

        [Test]
        public void Install_FileAlreadyIdenticalToPayload_IsLeftAloneAndAdopted()
        {
            WriteInstalled(ScriptPath, "-- shipped script v1"); // same content, put there by hand

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Updated, Does.Not.Contain(ScriptPath));
            Assert.That(report.UpdatedWithBackup, Is.Empty);
            Assert.That(BackupsOf(ScriptPath), Is.Empty);

            var manifest = PluginDeployManifest.Load(_engine);
            Assert.That(manifest.Files.ContainsKey(ScriptPath), Is.True);
        }

        [Test]
        public void Install_UserFileUnderAShippedName_IsTreatedAsTheirs()
        {
            WriteInstalled(ScriptPath, "-- a script of my own that happens to share the name");

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.UpdatedWithBackup.Count, Is.EqualTo(1));
            Assert.That(report.UpdatedWithBackup[0].RelativePath, Is.EqualTo(ScriptPath));
            Assert.That(File.ReadAllText(InstalledPath(ScriptPath) + ".old"),
                Is.EqualTo("-- a script of my own that happens to share the name"));
            Assert.That(InstalledText(ScriptPath), Is.EqualTo("-- shipped script v1"));
        }

        [Test]
        public void Install_FileDroppedFromBundle_Unmodified_IsRemoved()
        {
            PluginInstallation.Install(_engine, _payload);
            File.Delete(PayloadPath(ScriptPath));

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Removed, Does.Contain(ScriptPath));
            Assert.That(File.Exists(InstalledPath(ScriptPath)), Is.False);
            Assert.That(PluginDeployManifest.Load(_engine).Files.ContainsKey(ScriptPath), Is.False);
        }

        [Test]
        public void Install_FileDroppedFromBundle_Modified_IsKeptAndReleased()
        {
            PluginInstallation.Install(_engine, _payload);
            WriteInstalled(ScriptPath, "-- my own tweaks");
            File.Delete(PayloadPath(ScriptPath));

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Released, Does.Contain(ScriptPath));
            Assert.That(report.Removed, Does.Not.Contain(ScriptPath));
            Assert.That(InstalledText(ScriptPath), Is.EqualTo("-- my own tweaks"));
            Assert.That(PluginDeployManifest.Load(_engine).Files.ContainsKey(ScriptPath), Is.False);
        }

        [Test]
        public void Install_RunAgainWithTheSameBundle_DoesNothing()
        {
            PluginInstallation.Install(_engine, _payload);

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Success, Is.True);
            Assert.That(report.ChangedAnything, Is.False);
            Assert.That(BackupsOf(ScriptPath), Is.Empty);
        }

        [Test]
        public void Install_PluginItself_IsNeverBackedUp()
        {
            PluginInstallation.Install(_engine, _payload);
            WriteInstalled(PluginInstallation.PluginRelativePath, "someone replaced the dll");
            WritePayload(PluginInstallation.PluginRelativePath, "plugin v2");

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Updated, Does.Contain(PluginInstallation.PluginRelativePath));
            Assert.That(report.UpdatedWithBackup, Is.Empty);
            Assert.That(InstalledText(PluginInstallation.PluginRelativePath), Is.EqualTo("plugin v2"));
            Assert.That(BackupsOf(PluginInstallation.PluginRelativePath), Is.Empty);
        }

        [Test]
        public void Install_WithoutAManifest_TreatsWhatIsThereAsTheUsers()
        {
            PluginInstallation.Install(_engine, _payload);
            File.Delete(PluginDeployManifest.PathFor(_engine));
            WritePayload(ScriptPath, "-- shipped script v2");

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.UpdatedWithBackup.Count, Is.EqualTo(1));
            Assert.That(File.ReadAllText(InstalledPath(ScriptPath) + ".old"), Is.EqualTo("-- shipped script v1"));
        }

        [Test]
        public void Install_CorruptManifest_IsTreatedAsNoManifest()
        {
            PluginInstallation.Install(_engine, _payload);
            File.WriteAllText(PluginDeployManifest.PathFor(_engine), "{ this is not json");
            WritePayload(ScriptPath, "-- shipped script v2");

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Success, Is.True);
            Assert.That(report.UpdatedWithBackup.Count, Is.EqualTo(1));
            Assert.That(PluginDeployManifest.Load(_engine).Files.ContainsKey(ScriptPath), Is.True);
        }

        [Test]
        public void Install_IntoSomethingThatIsNotTheEngineFolder_Fails()
        {
            var notTheGame = Path.Combine(_root, "somewhere else");
            Directory.CreateDirectory(notTheGame);

            var report = PluginInstallation.Install(notTheGame, _payload);

            Assert.That(report.Success, Is.False);
            Assert.That(report.ChangedAnything, Is.False);
        }

        [Test]
        public void Install_WithoutAPayloadFolder_Fails()
        {
            var report = PluginInstallation.Install(_engine, Path.Combine(_root, "no payload here"));

            Assert.That(report.Success, Is.False);
            Assert.That(PluginInstallation.IsInstalled(_engine), Is.False);
        }

        [Test]
        public void Uninstall_TakesTheDllAndNothingElse()
        {
            PluginInstallation.Install(_engine, _payload);
            var skybox = Path.Combine(_engine, "TweakerStuff", "SkyboxReplacer", "Skyboxes", "mine.sky");
            Directory.CreateDirectory(Path.GetDirectoryName(skybox));
            File.WriteAllText(skybox, "a sky the user downloaded");

            var report = PluginInstallation.Uninstall(_engine);

            Assert.That(report.Success, Is.True);
            Assert.That(report.Removed, Does.Contain(PluginInstallation.PluginRelativePath));
            Assert.That(PluginInstallation.IsInstalled(_engine), Is.False);

            // Everything under TweakerStuff survives - scripts included, even though Tweaker put them there.
            Assert.That(File.Exists(InstalledPath(ScriptPath)), Is.True);
            Assert.That(File.Exists(skybox), Is.True);

            var manifest = PluginDeployManifest.Load(_engine);
            Assert.That(manifest.Files.ContainsKey(PluginInstallation.PluginRelativePath), Is.False);
            Assert.That(manifest.Files.ContainsKey(ScriptPath), Is.True);
        }

        [Test]
        public void Uninstall_WithNothingInstalled_DoesNotCreateAnything()
        {
            var report = PluginInstallation.Uninstall(_engine);

            Assert.That(report.Success, Is.True);
            Assert.That(report.ChangedAnything, Is.False);
            Assert.That(Directory.Exists(Path.Combine(_engine, PluginInstallation.StuffFolderName)), Is.False);
        }

        [Test]
        public void InstallThenUninstallThenInstall_PutsThePluginBack()
        {
            PluginInstallation.Install(_engine, _payload);
            PluginInstallation.Uninstall(_engine);

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Added, Does.Contain(PluginInstallation.PluginRelativePath));
            Assert.That(PluginInstallation.IsInstalled(_engine), Is.True);
        }

        [Test]
        public void EngineFolderFromTexturesPath_AnswersOnlyForTheRealThing()
        {
            var textures = Path.Combine(_engine, "textures");
            Directory.CreateDirectory(textures);

            Assert.That(PluginInstallation.EngineFolderFromTexturesPath(textures), Is.EqualTo(_engine));
            Assert.That(PluginInstallation.EngineFolderFromTexturesPath(textures + Path.DirectorySeparatorChar), Is.EqualTo(_engine));
            Assert.That(PluginInstallation.EngineFolderFromTexturesPath(Path.Combine(_root, "elsewhere", "textures")), Is.Null);
            Assert.That(PluginInstallation.EngineFolderFromTexturesPath(null), Is.Null);
            Assert.That(PluginInstallation.EngineFolderFromTexturesPath("   "), Is.Null);
        }

        /// <summary>
        /// The shape a hand-assembled bundle takes when the DLL is dropped at its top instead of into channels\.
        /// Left to run, it would install the DLL to engine\TweakerPlugin.dll - where the game never looks - and
        /// claim every other file it carries in the manifest, which is a licence to delete those later.
        /// </summary>
        [Test]
        public void Install_PayloadWithoutThePluginInChannels_RefusesAndChangesNothing()
        {
            File.Delete(PayloadPath(PluginInstallation.PluginRelativePath));
            WritePayload("TweakerPlugin.dll", "plugin v1, in the wrong place");

            var report = PluginInstallation.Install(_engine, _payload);

            Assert.That(report.Success, Is.False);
            Assert.That(report.Errors.Single().RelativePath, Is.EqualTo(PluginInstallation.PluginRelativePath));
            Assert.That(report.ChangedAnything, Is.False);
            Assert.That(File.Exists(InstalledPath("TweakerPlugin.dll")), Is.False);
            Assert.That(File.Exists(InstalledPath(ScriptPath)), Is.False);
            Assert.That(PluginDeployManifest.Load(_engine).Exists, Is.False);
            Assert.That(PluginInstallation.HasPlugin(_payload), Is.False);
        }

        /// <summary>A bundle Install would refuse can have no update pending, or startup keeps proposing one.</summary>
        [Test]
        public void NeedsUpdate_PayloadWithoutThePlugin_IsFalse()
        {
            File.Delete(PayloadPath(PluginInstallation.PluginRelativePath));

            Assert.That(PluginInstallation.NeedsUpdate(_engine, _payload), Is.False);
        }

        [Test]
        public void NeedsUpdate_NothingInstalledYet_IsTrue()
        {
            Assert.That(PluginInstallation.NeedsUpdate(_engine, _payload), Is.True);
        }

        [Test]
        public void NeedsUpdate_RightAfterInstall_IsFalse()
        {
            PluginInstallation.Install(_engine, _payload);

            Assert.That(PluginInstallation.NeedsUpdate(_engine, _payload), Is.False);
        }

        [Test]
        public void NeedsUpdate_NewerBundle_IsTrue()
        {
            PluginInstallation.Install(_engine, _payload);
            WritePayload(ScriptPath, "-- shipped script v2");

            Assert.That(PluginInstallation.NeedsUpdate(_engine, _payload), Is.True);
        }

        /// <summary>
        /// A shipped file the user deleted stays deleted (§6.3), so an update that would only "restore" it is not
        /// an update at all - and must not be a reason to ask them to close the game.
        /// </summary>
        [Test]
        public void NeedsUpdate_UserDeletedAShippedFile_IsFalse()
        {
            PluginInstallation.Install(_engine, _payload);
            File.Delete(InstalledPath(ScriptPath));

            Assert.That(PluginInstallation.NeedsUpdate(_engine, _payload), Is.False);
        }

        /// <summary>
        /// A file the user edited and the bundle no longer ships is only forgotten by the manifest - nothing on
        /// disk moves, so nothing needs the game closed.
        /// </summary>
        [Test]
        public void NeedsUpdate_DroppedButUserModifiedFile_IsFalse()
        {
            PluginInstallation.Install(_engine, _payload);
            File.Delete(PayloadPath(ScriptPath));
            WriteInstalled(ScriptPath, "-- my own version");

            Assert.That(PluginInstallation.NeedsUpdate(_engine, _payload), Is.False);
        }

        [Test]
        public void NeedsUpdate_DroppedAndUntouchedFile_IsTrue()
        {
            PluginInstallation.Install(_engine, _payload);
            File.Delete(PayloadPath(ScriptPath));

            Assert.That(PluginInstallation.NeedsUpdate(_engine, _payload), Is.True);
        }

        [Test]
        public void NeedsUpdate_NoEngineOrNoPayload_IsFalse()
        {
            Assert.That(PluginInstallation.NeedsUpdate(Path.Combine(_root, "nowhere"), _payload), Is.False);
            Assert.That(PluginInstallation.NeedsUpdate(_engine, Path.Combine(_root, "nowhere")), Is.False);
        }

        /// <summary>
        /// Both readers go through FileVersionInfo, which has nothing to say about the plain text files these
        /// tests ship - the point being that "no version" is a null, not a throw.
        /// </summary>
        [Test]
        public void Versions_AreNullForFilesWithoutAVersionResource()
        {
            Assert.That(PluginInstallation.InstalledVersion(_engine), Is.Null);
            PluginInstallation.Install(_engine, _payload);

            Assert.That(PluginInstallation.InstalledVersion(_engine), Is.Null);
            Assert.That(PluginInstallation.PayloadVersion(_payload), Is.Null);
            Assert.That(PluginInstallation.PayloadVersion(Path.Combine(_root, "nowhere")), Is.Null);
            Assert.That(PluginInstallation.PayloadVersion(null), Is.Null);
        }

        private string PayloadPath(string relativePath)
        {
            return Path.Combine(_payload, relativePath.Replace('/', Path.DirectorySeparatorChar));
        }

        private string InstalledPath(string relativePath)
        {
            return Path.Combine(_engine, relativePath.Replace('/', Path.DirectorySeparatorChar));
        }

        private string InstalledText(string relativePath)
        {
            return File.ReadAllText(InstalledPath(relativePath));
        }

        private void WritePayload(string relativePath, string content)
        {
            var path = PayloadPath(relativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, content);
        }

        private void WriteInstalled(string relativePath, string content)
        {
            var path = InstalledPath(relativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, content);
        }

        /// <summary>Every ".old*" file sitting next to the given installed file.</summary>
        private string[] BackupsOf(string relativePath)
        {
            var path = InstalledPath(relativePath);
            var directory = Path.GetDirectoryName(path);
            if (!Directory.Exists(directory))
                return Array.Empty<string>();

            return Directory.GetFiles(directory, Path.GetFileName(path) + ".old*").ToArray();
        }
    }
}
