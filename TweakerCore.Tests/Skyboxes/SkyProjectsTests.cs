namespace TweakerCore.Tests.Skyboxes
{
    using System;
    using System.IO;
    using System.Linq;
    using NUnit.Framework;
    using TweakerCore.Skyboxes;

    /// <summary>The sky project list, reading Config.json, editing its three fields, and the package name's rules.</summary>
    [TestFixture]
    public class SkyProjectsTests
    {
        private string _root;
        private string _sources;
        private string _recents;

        [SetUp]
        public void SetUp()
        {
            _root = Path.Combine(Path.GetTempPath(), "TweakerCoreTests", "SkyProjects", Guid.NewGuid().ToString("N"));
            _sources = Path.Combine(_root, "DevTools", "SkySources");
            _recents = Path.Combine(_root, "DevTools", "recent-sky-projects.txt");
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }

        [Test]
        public void TheListHasTheSourcesAndWhatWasOpenedFromElsewhere()
        {
            SkyTemplates.Create("minimal", Path.Combine(_sources, "Dawn"), "Dawn", "");
            SkyTemplates.Create("minimal", Path.Combine(_sources, "Abyss"), "Abyss", "");
            Directory.CreateDirectory(Path.Combine(_sources, "not a project"));
            var elsewhere = Path.Combine(_root, "git", "nebula");
            SkyTemplates.Create("minimal", elsewhere, "Nebula", "");
            var projects = new SkyProjects(_sources, _recents);

            Assert.That(projects.Remember(Path.Combine(_root, "git")), Is.False, "a folder without Config.json is not a project");
            Assert.That(projects.Remember(elsewhere), Is.True);

            var listed = projects.List();
            Assert.That(listed.Select(p => p.Name), Is.EqualTo(new[] { "Nebula", "Abyss", "Dawn" }));
            Assert.That(listed.Select(p => p.InSources), Is.EqualTo(new[] { false, true, true }));
        }

        [Test]
        public void ReadingTellsWhatIsWrongWithoutHidingTheProject()
        {
            var project = Path.Combine(_root, "broken");
            Directory.CreateDirectory(project);
            File.WriteAllText(Path.Combine(project, "Config.json"), "{ \"name\": ");

            Assert.That(SkyProjects.Read(project, false).ManifestProblem, Does.StartWith("Config.json is not valid JSON"));

            File.WriteAllText(Path.Combine(project, "Config.json"), "{ \"layers\": [] }");
            Assert.That(SkyProjects.Read(project, false).ManifestProblem, Does.Contain("has no \"name\""));

            File.WriteAllText(Path.Combine(project, "Config.json"), "{ \"name\": \"A:B\", \"version\": 3, \"layers\": [ {}, {} ] }");
            var read = SkyProjects.Read(project, false);
            Assert.That(read.ManifestProblem, Is.Null);
            Assert.That(read.NameProblem, Does.StartWith("It is a file name"));
            Assert.That(read.PackageFileName, Is.Null);
            Assert.That(read.Version, Is.EqualTo(3));
            Assert.That(read.LayerCount, Is.EqualTo(2));

            Assert.That(SkyProjects.Read(Path.Combine(_root, "gone"), false).IsMissing, Is.True);
        }

        [Test]
        public void TheThreeFieldsAreEditedInTheText()
        {
            var project = Path.Combine(_root, "p");
            SkyTemplates.Create("gradient", project, "Before", "");

            SkyManifest.Update(project, "After", "Вася", 2);

            var read = SkyProjects.Read(project, false);
            Assert.That((read.Name, read.Author, read.Version), Is.EqualTo(("After", "Вася", (int?)2)));
            Assert.That(File.ReadAllText(Path.Combine(project, "Config.json")), Does.Contain("// Not a release number"));
        }

        [TestCase("My Sky", null)]
        [TestCase("Закат v1.2", null)]
        [TestCase("", "Must not be empty.")]
        [TestCase(" Padded", "No spaces at the start or the end.")]
        [TestCase("Dots.", "Must not end with a dot - Windows drops it from a file name.")]
        [TestCase("a/b", "It is a file name, so none of \\ / : * ? \" < > |")]
        [TestCase("nul", "nul is a device name in Windows and cannot be a file name.")]
        [TestCase("com1.backup", "com1 is a device name in Windows and cannot be a file name.")]
        [TestCase("Sky.sky", "Leave out .sky - it is added when the sky is installed.")]
        public void ThePackageNameIsAFileName(string name, string problem)
        {
            Assert.That(SkyPackageName.Problem(name), Is.EqualTo(problem));
        }

        [TestCase("1", null)]
        [TestCase(" 12 ", null)]
        [TestCase("1.0", "A whole number, like 1.")]
        [TestCase("-1", "A whole number, like 1.")]
        public void TheVersionIsAWholeNumber(string text, string problem)
        {
            Assert.That(SkyManifest.VersionProblem(text), Is.EqualTo(problem));
        }
    }
}
