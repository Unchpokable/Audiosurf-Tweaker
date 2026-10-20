namespace TweakerCore.Tests.Scripting
{
    using System;
    using System.IO;
    using System.Linq;
    using System.Text.Json;
    using NUnit.Framework;
    using TweakerCore.Scripting;

    /// <summary>
    /// AgentSetup over a small kit of its own and a scratch project (dev-bundle-roadmap.md §3.6, §5 "AgentSetup"):
    /// setting up, setting up again, a kit that moved on, a file the user edited, one they deleted, AGENTS.md with
    /// their text around our block, changing the agents chosen, a checkout with CRLF. The last test runs the real
    /// Tools/AgentKit, so a kit that AgentSetup cannot read fails here and not on a user's machine.
    /// </summary>
    [TestFixture]
    public class AgentSetupTests
    {
        private static readonly AgentChoice Claude = new AgentChoice(true, true, true, false);
        private static readonly AgentChoice NoClaude = new AgentChoice(false, true, true, false);

        private string _root;
        private string _kitRoot;
        private string _docs;
        private string _project;

        [SetUp]
        public void SetUp()
        {
            _root = Path.Combine(Path.GetTempPath(), "TweakerCoreTests", "AgentSetup", Guid.NewGuid().ToString("N"));
            _kitRoot = Path.Combine(_root, "DevTools", "AgentKit");
            _docs = Path.Combine(_root, "DevTools", "Docs");
            _project = Path.Combine(_root, "my_hud");

            Put(_kitRoot, "kit.json", "{ \"format\": 1, \"version\": 1, \"skills\": [ { \"name\": \"alpha\" }, { \"name\": \"beta\", \"optional\": true } ] }");
            Put(_kitRoot, "agents/agents-block.md", "## Block\n\nAlpha is a skill.\n");
            Put(_kitRoot, "agents/AGENTS.md", "# {{project-name}}\n\n<!-- tweaker:begin -->\n{{block}}\n<!-- tweaker:end -->\n");
            Put(_kitRoot, "agents/CLAUDE.md", "@AGENTS.md\n");
            Put(_kitRoot, "agents/claude-skill.md", "---\nname: {{name}}\ndescription: {{description}}\n---\n\nRead .agents/skills/{{name}}/SKILL.md\n");
            Put(_kitRoot, "skills/alpha/SKILL.md", "---\nname: alpha\ndescription: \"Alpha: does the first thing\"\n---\n\nBody v1\n");
            Put(_kitRoot, "skills/alpha/references/more.md", "More v1\n");
            Put(_kitRoot, "skills/beta/SKILL.md", "---\nname: beta\ndescription: Beta, optional\n---\n\nBeta\n");
            Put(_docs, "scripting.md", "# Scripting\n");
            File.WriteAllBytes(Path.Combine(_docs, "pic.png"), new byte[] { 0x89, 0x50, 0x0d, 0x0a, 0x1a, 0x0a });

            Put(_project, "twproject.json", "{ \"id\": \"my_hud\" }");
            Put(_project, ".gitignore", "build/\n");
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }

        private static void Put(string root, string relative, string text)
        {
            var path = Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, text);
        }

        private string Read(string relative) => File.ReadAllText(Path.Combine(_project, relative.Replace('/', Path.DirectorySeparatorChar)));

        private bool Has(string relative) => File.Exists(Path.Combine(_project, relative.Replace('/', Path.DirectorySeparatorChar)));

        private AgentKit Kit()
        {
            var kit = AgentKit.Load(_kitRoot, out var problem);
            Assert.That(problem, Is.Null);
            return kit;
        }

        private AgentSetupReport Apply(AgentChoice choice, bool replaceEdited = false)
        {
            var report = AgentSetup.Apply(_project, "My HUD", choice, Kit(), _docs, @"D:\Games\Audiosurf", @"C:\Tweaker\DevTools", replaceEdited);
            Assert.That(report.Errors, Is.Empty);
            return report;
        }

        private AgentSetupStatus Status() => AgentSetup.Status(_project, Kit(), _docs);

        [Test]
        public void AFreshSetupWritesEverythingTheChoiceAsksFor()
        {
            Assert.That(Status().State, Is.EqualTo(AgentSetupState.NotSetUp));

            Apply(Claude);

            Assert.Multiple(() =>
            {
                Assert.That(Read(".agents/skills/alpha/SKILL.md"), Does.Contain("Body v1"));
                Assert.That(Read(".agents/skills/alpha/references/more.md"), Is.EqualTo("More v1\n"));
                Assert.That(Has(".agents/skills/beta/SKILL.md"), Is.False, "the optional skill only when asked for");
                Assert.That(Read(".claude/skills/alpha/SKILL.md"), Does.Contain("description: \"Alpha: does the first thing\"").And.Contain(".agents/skills/alpha/SKILL.md"),
                    "Claude's pointer carries the description verbatim, quotes and all");
                Assert.That(Read("AGENTS.md"), Is.EqualTo("# My HUD\n\n<!-- tweaker:begin -->\n## Block\n\nAlpha is a skill.\n<!-- tweaker:end -->\n"));
                Assert.That(Read("CLAUDE.md"), Is.EqualTo("@AGENTS.md\n"));
                Assert.That(Read(".tweaker/docs/scripting.md"), Is.EqualTo("# Scripting\n"));
                Assert.That(Has(".tweaker/docs/pic.png"), Is.True);
                Assert.That(Read(".gitignore"), Does.StartWith("build/\n").And.Contain("\n.tweaker/local.json\n"), "the user's lines stay, ours is added");
                Assert.That(Has(".tweaker/manifest.json"), Is.True);
            });

            using var local = JsonDocument.Parse(Read(".tweaker/local.json"));
            Assert.That(local.RootElement.GetProperty("game").GetString(), Is.EqualTo(@"D:\Games\Audiosurf"));
            Assert.That(local.RootElement.GetProperty("devtools").GetString(), Is.EqualTo(@"C:\Tweaker\DevTools"));

            var status = Status();
            Assert.That(status.State, Is.EqualTo(AgentSetupState.UpToDate));
            Assert.That(status.Choice, Is.EqualTo(Claude), "the choice is remembered in the project");
            Assert.That(AgentSetup.ChoiceOf(_project), Is.EqualTo(Claude));
        }

        [Test]
        public void SettingUpAgainChangesNothing()
        {
            Apply(Claude);
            var manifest = Read(".tweaker/manifest.json");
            var gitignore = Read(".gitignore");

            var again = Apply(Claude);

            Assert.That(again.Added.Concat(again.Updated).Concat(again.Removed).Concat(again.KeptEdited), Is.Empty);
            Assert.That(Read(".tweaker/manifest.json"), Is.EqualTo(manifest));
            Assert.That(Read(".gitignore"), Is.EqualTo(gitignore), "the gitignore line is added once");
        }

        [Test]
        public void ANewerKitReplacesWhatTheUserDidNotTouch()
        {
            Apply(Claude);
            Put(_kitRoot, "skills/alpha/SKILL.md", "---\nname: alpha\ndescription: \"Alpha: does the first thing\"\n---\n\nBody v2\n");
            Put(_kitRoot, "kit.json", "{ \"format\": 1, \"version\": 2, \"skills\": [ { \"name\": \"alpha\" }, { \"name\": \"beta\", \"optional\": true } ] }");

            var status = Status();
            Assert.That(status.State, Is.EqualTo(AgentSetupState.UpdateAvailable));
            Assert.That(status.Pending, Is.EqualTo(new[] { ".agents/skills/alpha/SKILL.md" }));
            Assert.That(status.InstalledKitVersion, Is.EqualTo(1));

            var report = Apply(Claude);
            Assert.That(report.Updated, Is.EqualTo(new[] { ".agents/skills/alpha/SKILL.md" }));
            Assert.That(Read(".agents/skills/alpha/SKILL.md"), Does.Contain("Body v2"));
            Assert.That(Status().State, Is.EqualTo(AgentSetupState.UpToDate));
        }

        [Test]
        public void AFileTheUserEditedIsKeptUntilTheyAskForTheKitsVersion()
        {
            Apply(Claude);
            Put(_project, ".agents/skills/alpha/SKILL.md", "my own notes\n");
            Put(_kitRoot, "skills/alpha/SKILL.md", "---\nname: alpha\ndescription: x\n---\n\nBody v2\n");

            var status = Status();
            Assert.That(status.Edited, Is.EqualTo(new[] { ".agents/skills/alpha/SKILL.md" }));

            var kept = Apply(Claude);
            Assert.That(kept.KeptEdited, Is.EqualTo(new[] { ".agents/skills/alpha/SKILL.md" }));
            Assert.That(Read(".agents/skills/alpha/SKILL.md"), Is.EqualTo("my own notes\n"));
            Assert.That(Status().Edited, Is.Not.Empty, "still reported after an update - it is still edited");

            var replaced = Apply(Claude, replaceEdited: true);
            Assert.That(replaced.BackedUp.Select(b => b.Path), Is.EqualTo(new[] { ".agents/skills/alpha/SKILL.md" }));
            Assert.That(Read(".agents/skills/alpha/SKILL.md"), Does.Contain("Body v2"));
            Assert.That(Read(replaced.BackedUp[0].Backup), Is.EqualTo("my own notes\n"), "their version survives as a .old copy");
            Assert.That(Status().Edited, Is.Empty);
        }

        [Test]
        public void AFileTheUserDeletedStaysDeleted()
        {
            Apply(Claude);
            File.Delete(Path.Combine(_project, ".agents", "skills", "alpha", "references", "more.md"));

            Assert.That(Status().State, Is.EqualTo(AgentSetupState.UpToDate), "a deletion is not an update to offer");
            var report = Apply(Claude);
            Assert.That(report.SkippedDeleted, Is.EqualTo(new[] { ".agents/skills/alpha/references/more.md" }));
            Assert.That(Has(".agents/skills/alpha/references/more.md"), Is.False);
        }

        [Test]
        public void TheUsersTextAroundTheBlockSurvivesAndTheBlockIsReplaced()
        {
            Apply(Claude);
            var agents = Read("AGENTS.md").Replace("# My HUD\n", "# My HUD\n\nMy rules come first.\n") + "\nAnd a note after.\n";
            Put(_project, "AGENTS.md", agents);
            Put(_kitRoot, "agents/agents-block.md", "## Block v2\n");

            Apply(Claude);

            Assert.That(Read("AGENTS.md"), Is.EqualTo("# My HUD\n\nMy rules come first.\n\n<!-- tweaker:begin -->\n## Block v2\n<!-- tweaker:end -->\n\nAnd a note after.\n"));
        }

        [Test]
        public void AnEditInsideTheBlockIsTheUsersUntilTheyAskForTheKitsVersion()
        {
            Apply(Claude);
            Put(_project, "AGENTS.md", Read("AGENTS.md").Replace("Alpha is a skill.", "Alpha is MY skill."));
            Put(_kitRoot, "agents/agents-block.md", "## Block v2\n");

            Assert.That(Apply(Claude).KeptEdited, Is.EqualTo(new[] { "AGENTS.md" }));
            Assert.That(Read("AGENTS.md"), Does.Contain("Alpha is MY skill."));

            var replaced = Apply(Claude, replaceEdited: true);
            Assert.That(Read("AGENTS.md"), Does.Contain("## Block v2").And.Not.Contain("MY skill"));
            Assert.That(Read(replaced.BackedUp.Single().Backup), Does.Contain("Alpha is MY skill."));
        }

        [Test]
        public void AnAgentsMdOfTheUsersGetsTheBlockAppendedAndKeepsItsLineEnds()
        {
            Put(_project, "AGENTS.md", "# Mine\r\n\r\nAlready here.\r\n");

            Apply(NoClaude);

            Assert.That(Read("AGENTS.md"), Is.EqualTo("# Mine\r\n\r\nAlready here.\r\n\r\n<!-- tweaker:begin -->\r\n## Block\r\n\r\nAlpha is a skill.\r\n<!-- tweaker:end -->\r\n"));
            Assert.That(Status().State, Is.EqualTo(AgentSetupState.UpToDate));
        }

        [Test]
        public void MarkersTheUserTookOutAreNotPutBack()
        {
            Apply(Claude);
            Put(_project, "AGENTS.md", "# Only mine now\n");

            var report = Apply(Claude);

            Assert.That(report.SkippedDeleted, Does.Contain("AGENTS.md"));
            Assert.That(Read("AGENTS.md"), Is.EqualTo("# Only mine now\n"));
        }

        [Test]
        public void ChangingTheChoiceAddsAndRemovesOnlyWhatItCovers()
        {
            Apply(Claude);
            Put(_project, ".claude/settings.json", "{}");

            var withBinaries = Apply(new AgentChoice(false, true, true, true));

            Assert.Multiple(() =>
            {
                Assert.That(Has(".agents/skills/beta/SKILL.md"), Is.True, "the optional skill comes in when chosen");
                Assert.That(withBinaries.Removed, Does.Contain(".claude/skills/alpha/SKILL.md"));
                Assert.That(Directory.Exists(Path.Combine(_project, ".claude", "skills")), Is.False, "emptied folders go too");
                Assert.That(Has(".claude/settings.json"), Is.True, "but not what the user keeps there");
                Assert.That(Has("CLAUDE.md"), Is.True, "CLAUDE.md is the user's once written");
            });

            Apply(NoClaude);
            Assert.That(Has(".agents/skills/beta/SKILL.md"), Is.False, "and goes again when unchosen");
        }

        [Test]
        public void AnExistingClaudeMdGetsTheImportOnceAndOnlyOnce()
        {
            Put(_project, "CLAUDE.md", "My Claude notes.\n");

            Apply(Claude);
            Assert.That(Read("CLAUDE.md"), Is.EqualTo("@AGENTS.md\n\nMy Claude notes.\n"));

            Put(_project, "CLAUDE.md", "My Claude notes, and I took the import out.\n");
            Apply(Claude);
            Assert.That(Read("CLAUDE.md"), Is.EqualTo("My Claude notes, and I took the import out.\n"));
        }

        [Test]
        public void ACheckoutWithCrlfIsNotAnEdit()
        {
            Apply(Claude);
            foreach (var file in Directory.EnumerateFiles(_project, "*.md", SearchOption.AllDirectories))
                File.WriteAllText(file, File.ReadAllText(file).Replace("\n", "\r\n"));

            var status = Status();

            Assert.That(status.Edited, Is.Empty);
            Assert.That(status.State, Is.EqualTo(AgentSetupState.UpToDate));
        }

        [Test]
        public void ASkillTheKitDroppedGoesUnlessTheUserEditedIt()
        {
            Apply(Claude);
            Put(_project, ".agents/skills/alpha/references/more.md", "edited\n");
            Put(_kitRoot, "kit.json", "{ \"format\": 1, \"version\": 2, \"skills\": [ { \"name\": \"beta\", \"optional\": true } ] }");

            var report = Apply(new AgentChoice(true, true, true, true));

            Assert.That(report.Removed, Does.Contain(".agents/skills/alpha/SKILL.md").And.Contain(".claude/skills/alpha/SKILL.md"));
            Assert.That(Read(".agents/skills/alpha/references/more.md"), Is.EqualTo("edited\n"), "an edited file stays, as the user's");
            Assert.That(Status().State, Is.EqualTo(AgentSetupState.UpToDate), "and is no longer ours to report");
        }

        [Test]
        public void LocalJsonIsOnlyForProjectsThatAreSetUp()
        {
            AgentSetup.WriteLocal(_project, @"D:\Games\Audiosurf", @"C:\Tweaker\DevTools");
            Assert.That(Directory.Exists(Path.Combine(_project, ".tweaker")), Is.False);

            Apply(Claude);
            AgentSetup.WriteLocal(_project, null, @"E:\Elsewhere\DevTools");
            using var local = JsonDocument.Parse(Read(".tweaker/local.json"));
            Assert.That(local.RootElement.GetProperty("game").ValueKind, Is.EqualTo(JsonValueKind.Null), "an unknown game is null, for the skill to ask about");
            Assert.That(local.RootElement.GetProperty("devtools").GetString(), Is.EqualTo(@"E:\Elsewhere\DevTools"));
        }

        [Test]
        public void ABrokenKitSaysWhatIsWrong()
        {
            Assert.That(AgentKit.Load(Path.Combine(_root, "nowhere"), out var missing), Is.Null);
            Assert.That(missing, Does.Contain("kit.json"));

            File.Delete(Path.Combine(_kitRoot, "skills", "beta", "SKILL.md"));
            Assert.That(AgentKit.Load(_kitRoot, out var damaged), Is.Null);
            Assert.That(damaged, Does.Contain("beta"));
        }

        [Test]
        public void TheShippedKitSetsUpAProject()
        {
            var repo = new DirectoryInfo(TestContext.CurrentContext.TestDirectory);
            while (repo != null && !File.Exists(Path.Combine(repo.FullName, "Audiosurf SkinChanger.sln")))
                repo = repo.Parent;
            if (repo == null)
                Assert.Inconclusive("repository not found");

            var kit = AgentKit.Load(Path.Combine(repo.FullName, "Tools", "AgentKit"), out var problem);
            Assert.That(problem, Is.Null);
            Assert.That(kit.Skills.All(s => AgentKit.Description(s)?.Length > 0), Is.True, "every skill has a description for Claude's pointer");

            var report = AgentSetup.Apply(_project, "My HUD", new AgentChoice(true, true, true, true), kit, Path.Combine(repo.FullName, "Docs"), null, @"C:\Tweaker\DevTools");

            Assert.That(report.Errors, Is.Empty);
            Assert.That(kit.Skills.All(s => Has(".agents/skills/" + s.Name + "/SKILL.md") && Has(".claude/skills/" + s.Name + "/SKILL.md")), Is.True);
            Assert.That(Read("AGENTS.md"), Does.StartWith("# My HUD").And.Contain("audiosurf-grill").And.Not.Contain("{{"));
            Assert.That(AgentSetup.Status(_project, kit, Path.Combine(repo.FullName, "Docs")).State, Is.EqualTo(AgentSetupState.UpToDate));
        }
    }
}
