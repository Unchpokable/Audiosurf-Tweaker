namespace TweakerCore.Tests.Skyboxes
{
    using System;
    using System.IO;
    using System.IO.Compression;
    using System.Linq;
    using NUnit.Framework;
    using TweakerCore.Skyboxes;

    /// <summary>
    /// Every template makes a project that passes the check before packing with nothing to say, carries the name it was
    /// given, gets the editor's files, and packs without them. Whether the shaders compile and the generator runs is the
    /// plugin's offline harness's to check - it loads these same files.
    /// </summary>
    [TestFixture]
    public class SkyTemplatesTests
    {
        private string _root;

        [SetUp]
        public void SetUp() => _root = Path.Combine(Path.GetTempPath(), "TweakerCoreTests", "SkyTemplates", Guid.NewGuid().ToString("N"));

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(_root))
                Directory.Delete(_root, recursive: true);
        }

        [TestCaseSource(nameof(Keys))]
        public void ATemplateMakesAProjectWithNothingToFix(string key)
        {
            var project = Path.Combine(_root, "Sunset Над Морем");
            SkyTemplates.Create(key, project, "Sunset Над Морем", "Вася");

            var read = SkyProjects.Read(project, inSources: false);
            Assert.That(read.ManifestProblem, Is.Null);
            Assert.That(read.Name, Is.EqualTo("Sunset Над Морем"));
            Assert.That(read.Author, Is.EqualTo("Вася"));
            Assert.That(read.Version, Is.EqualTo(1));
            Assert.That(read.PackageFileName, Is.EqualTo("Sunset Над Морем.sky"));
            Assert.That(SkyPackager.Check(project), Is.Empty, "a fresh template has nothing to warn about");
        }

        [TestCaseSource(nameof(Keys))]
        public void ANewProjectGetsTheEditorsFilesAndTheyStayOutOfThePackage(string key)
        {
            var project = Path.Combine(_root, key);
            SkyTemplates.Create(key, project, "Test", "");

            Assert.That(File.Exists(Path.Combine(project, ".tweaker", "include", "sky_common.hlsli")), Is.True);
            Assert.That(File.Exists(Path.Combine(project, "shadertoolsconfig.json")), Is.True);
            Assert.That(File.Exists(Path.Combine(project, ".vscode", "extensions.json")), Is.True);
            Assert.That(File.ReadAllText(Path.Combine(project, ".gitignore")), Does.Contain(".tweaker/"));

            var packaged = SkyPackager.PackageFiles(project);
            Assert.That(packaged, Does.Contain("Config.json"));
            Assert.That(packaged.Where(file => file.StartsWith(".") || file.Contains("/.") || file == "shadertoolsconfig.json"), Is.Empty,
                "nothing of the editor's goes into the sky: " + string.Join(", ", packaged));

            using var zip = new MemoryStream();
            SkyPackager.Pack(project, zip, CompressionLevel.Fastest);
            zip.Position = 0;
            using var archive = new ZipArchive(zip, ZipArchiveMode.Read);
            Assert.That(archive.Entries.Select(e => e.FullName).OrderBy(n => n, StringComparer.OrdinalIgnoreCase), Is.EqualTo(packaged));
        }

        [Test]
        public void AnEmptyAuthorStaysEmptyAndTheManifestKeepsItsComments()
        {
            var project = Path.Combine(_root, "gradient");
            SkyTemplates.Create("gradient", project, "Plain", "");

            var text = File.ReadAllText(Path.Combine(project, "Config.json"));
            Assert.That(text, Does.Contain("\"name\": \"Plain\""));
            Assert.That(text, Does.Contain("\"author\": \"\""));
            Assert.That(text, Does.Contain("// The name is also the file this sky is installed as"));
        }

        [Test]
        public void ANonEmptyFolderIsRefused()
        {
            var project = Path.Combine(_root, "taken");
            Directory.CreateDirectory(project);
            File.WriteAllText(Path.Combine(project, "mine.txt"), "x");

            Assert.Throws<IOException>(() => SkyTemplates.Create("gradient", project, "Taken", ""));
            Assert.That(File.Exists(Path.Combine(project, "Config.json")), Is.False);
        }

        [Test]
        public void TheEditorsHeadersAreThePluginsOwnByteForByte()
        {
            var shaders = Path.GetFullPath(Path.Combine(TestContext.CurrentContext.TestDirectory, "..", "..", "..", "..", "..", "TweakerPlugin", "assets", "shaders"));
            var expected = Directory.GetFiles(shaders, "*.hlsli").ToDictionary(Path.GetFileName, File.ReadAllBytes);

            Assert.That(SkyEditorSupport.Headers.Keys, Is.EquivalentTo(expected.Keys));
            foreach (var header in expected)
                Assert.That(SkyEditorSupport.Headers[header.Key], Is.EqualTo(header.Value), header.Key);
        }

        [Test]
        public void RefreshingAnUpToDateProjectWritesNothing()
        {
            var project = Path.Combine(_root, "gradient");
            SkyTemplates.Create("gradient", project, "Plain", "");

            Assert.That(SkyEditorSupport.Refresh(project), Is.False, "a project opened again must not look saved to the watcher");

            File.WriteAllText(Path.Combine(project, ".tweaker", "include", "sky_common.hlsli"), "stale");
            File.WriteAllText(Path.Combine(project, "shadertoolsconfig.json"), "{ \"mine\": true }");
            Assert.That(SkyEditorSupport.Refresh(project), Is.True);
            Assert.That(File.ReadAllBytes(Path.Combine(project, ".tweaker", "include", "sky_common.hlsli")), Is.EqualTo(SkyEditorSupport.Headers["sky_common.hlsli"]));
            Assert.That(File.ReadAllText(Path.Combine(project, "shadertoolsconfig.json")), Is.EqualTo("{ \"mine\": true }"), "the user's config stays theirs");
        }

        [Test]
        public void TheGitignoreLineIsAddedOnceToWhatIsThere()
        {
            var project = Path.Combine(_root, "git");
            Directory.CreateDirectory(project);
            File.WriteAllText(Path.Combine(project, ".gitignore"), "*.psd");

            SkyEditorSupport.SetUpNew(project);
            SkyEditorSupport.SetUpNew(project);

            Assert.That(File.ReadAllText(Path.Combine(project, ".gitignore")), Is.EqualTo("*.psd\n.tweaker/\n"));
        }

        private static string[] Keys() => SkyTemplates.All.Select(t => t.Key).ToArray();
    }
}
