namespace TweakerCore.Tests.Scripting
{
    using System;
    using System.IO;
    using System.Linq;
    using NUnit.Framework;
    using TweakerCore.Scripting;

    [TestFixture]
    public class ScriptProjectsTests
    {
        private string _root;
        private string _sources;
        private string _recents;

        [SetUp]
        public void SetUp()
        {
            _root = Path.Combine(Path.GetTempPath(), "TweakerCoreTests", "ScriptProjects", Guid.NewGuid().ToString("N"));
            _sources = Path.Combine(_root, "DevTools", "Sources");
            _recents = Path.Combine(_root, "DevTools", "recent-projects.txt");
            Directory.CreateDirectory(_sources);
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }

        private string Project(string folder, string manifest = null)
        {
            Directory.CreateDirectory(folder);
            File.WriteAllText(Path.Combine(folder, ScriptProjects.ManifestFileName),
                manifest ?? "{\n  // a comment, as twllink allows\n  \"id\": \"" + Path.GetFileName(folder) + "\", \"name\": \"N\", \"version\": \"1.0.0\",\n}");
            return folder;
        }

        [Test]
        public void ListsTheProjectsInTheSourcesFolder()
        {
            Project(Path.Combine(_sources, "b_hud"));
            Project(Path.Combine(_sources, "a_hook"));
            Directory.CreateDirectory(Path.Combine(_sources, "not_a_project"));

            var list = new ScriptProjects(_sources, _recents).List();

            Assert.That(list.Select(p => p.Id), Is.EqualTo(new[] { "a_hook", "b_hud" }));
            Assert.That(list.All(p => p.InSources && p.ManifestProblem == null), Is.True);
            Assert.That(list[0].ScriptFileName, Is.EqualTo("a_hook.lua"));
        }

        [Test]
        public void RememberedProjectsComeFirstNewestFirstAndSurviveARestart()
        {
            var elsewhere = Project(Path.Combine(_root, "work", "far_hud"));
            var inSources = Project(Path.Combine(_sources, "near_hud"));
            var projects = new ScriptProjects(_sources, _recents);

            Assert.That(projects.Remember(elsewhere), Is.True);
            Assert.That(projects.Remember(inSources), Is.True);

            var list = new ScriptProjects(_sources, _recents).List();
            Assert.That(list.Select(p => p.Id), Is.EqualTo(new[] { "near_hud", "far_hud" }), "no duplicate for a remembered project in Sources");
            Assert.That(list[1].InSources, Is.False);
        }

        [Test]
        public void AFolderWithoutAManifestIsNotRemembered()
        {
            var plain = Path.Combine(_root, "plain");
            Directory.CreateDirectory(plain);
            var projects = new ScriptProjects(_sources, _recents);

            Assert.That(projects.Remember(plain), Is.False);
            Assert.That(projects.Recent, Is.Empty);
            Assert.That(File.Exists(_recents), Is.False);
        }

        [Test]
        public void AVanishedRememberedFolderIsListedAsMissingUntilForgotten()
        {
            var far = Project(Path.Combine(_root, "work", "far_hud"));
            var projects = new ScriptProjects(_sources, _recents);
            projects.Remember(far);
            Directory.Delete(far, recursive: true);

            var missing = projects.List().Single();
            Assert.That(missing.IsMissing, Is.True);

            projects.Forget(far + Path.DirectorySeparatorChar);
            Assert.That(new ScriptProjects(_sources, _recents).List(), Is.Empty);
        }

        [Test]
        public void ADeletedProjectInSourcesIsDroppedNotListedAsMissing()
        {
            var projects = new ScriptProjects(_sources, _recents);
            var deleted = Project(Path.Combine(_sources, "deleted_hud"));
            projects.Remember(deleted);
            Directory.Delete(deleted, recursive: true);

            Assert.That(projects.List(), Is.Empty);
            Assert.That(File.ReadAllText(_recents).Trim(), Is.Empty, "and it is gone from the recents file");
        }

        [Test]
        public void TheRecentsFileIsReadAgainOnEveryList()
        {
            var far = Project(Path.Combine(_root, "work", "far_hud"));
            var projects = new ScriptProjects(_sources, _recents);
            projects.Remember(far);

            File.WriteAllText(_recents, "");

            Assert.That(projects.List(), Is.Empty, "a line removed by hand is gone without a restart");
        }

        [Test]
        public void RememberKeepsALineAddedByHand()
        {
            var first = Project(Path.Combine(_root, "work", "first"));
            var byHand = Project(Path.Combine(_root, "work", "by_hand"));
            var projects = new ScriptProjects(_sources, _recents);
            projects.Remember(first);

            File.AppendAllText(_recents, byHand + "\n");
            projects.Remember(first);

            Assert.That(new ScriptProjects(_sources, _recents).Recent.Select(Path.GetFileName), Is.EqualTo(new[] { "first", "by_hand" }));
        }

        [Test]
        public void RecentsAreCapped()
        {
            var projects = new ScriptProjects(_sources, _recents);
            for (var i = 0; i < ScriptProjects.RecentLimit + 5; i++)
                projects.Remember(Project(Path.Combine(_root, "work", "p" + i)));

            Assert.That(projects.Recent.Count, Is.EqualTo(ScriptProjects.RecentLimit));
            Assert.That(Path.GetFileName(projects.Recent[0]), Is.EqualTo("p" + (ScriptProjects.RecentLimit + 4)));
        }

        [TestCase("{ not json", "not valid JSON")]
        [TestCase("[1, 2]", "not a JSON object")]
        [TestCase("{ \"name\": \"no id\" }", "has no \"id\"")]
        public void ABrokenManifestIsStillListedWithTheReason(string manifest, string reason)
        {
            Project(Path.Combine(_sources, "broken"), manifest);

            var project = new ScriptProjects(_sources, _recents).List().Single();
            Assert.That(project.ManifestProblem, Does.Contain(reason));
            Assert.That(project.Id, Is.Null);
            Assert.That(project.ScriptFileName, Is.Null);
        }
    }
}
