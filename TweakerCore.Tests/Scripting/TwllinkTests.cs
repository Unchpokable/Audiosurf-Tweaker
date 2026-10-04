namespace TweakerCore.Tests.Scripting
{
    using System.IO;
    using System.Linq;
    using System.Threading.Tasks;
    using NUnit.Framework;
    using TweakerCore.Scripting;

    /// <summary>
    /// The client against the real twllink, built by this project's reference to TweakerLuaLinker and copied
    /// into twllink\ of the test output (TweakerCore.Tests.csproj).
    /// </summary>
    [TestFixture]
    public class TwllinkTests
    {
        private ScratchGame _game;
        private Twllink _twllink;

        [SetUp]
        public void SetUp()
        {
            _game = new ScratchGame();
            _twllink = new Twllink(Path.Combine(TestContext.CurrentContext.TestDirectory, "twllink", Twllink.ExecutableName));
            Assert.That(_twllink.Exists, Is.True, "twllink is copied next to the tests by the build");
        }

        [TearDown]
        public void TearDown() => _game.Dispose();

        private string ProjectPath(string id) => Path.Combine(_game.Root, "Sources", id);

        private async Task<string> NewProject(string id, string template = "hud")
        {
            var path = ProjectPath(id);
            var result = await _twllink.NewAsync(template, path);
            Assert.That(result.ExitCode, Is.EqualTo(TwllinkResult.ExitSuccess), result.ToolMessage);
            return path;
        }

        [Test]
        public async Task ReportsItsVersion()
        {
            Assert.That(await _twllink.VersionAsync(), Does.Match(@"^\d+\.\d+$"));
        }

        [Test]
        public async Task ANewProjectBuilds()
        {
            var project = await NewProject("my_hud");

            var result = await _twllink.BuildAsync(project);

            Assert.That(result.Success, Is.True, result.ToolMessage);
            Assert.That(result.ToolMessage, Is.Null);
            Assert.That(result.Build.Id, Is.EqualTo("my_hud"));
            Assert.That(result.Build.Errors, Is.Zero);
            Assert.That(result.Build.Modules, Does.Contain("main"));
            Assert.That(File.Exists(result.Build.Output), Is.True);
            Assert.That(File.Exists(result.Build.Map), Is.True);
            Assert.That(result.Build.Installed, Is.Null);
        }

        [Test]
        public async Task ErrorsInTheProjectComeBackAsDiagnostics()
        {
            var project = await NewProject("broken", "empty");
            File.WriteAllText(Path.Combine(project, "src", "main.lua"), "local f = io.open('x')\n");

            var result = await _twllink.BuildAsync(project);

            Assert.That(result.ExitCode, Is.EqualTo(TwllinkResult.ExitBuildFailed));
            Assert.That(result.Build.Output, Is.Null);
            var error = result.Build.Diagnostics.Single(d => d.Severity == TwllinkSeverity.Error);
            Assert.That(error.Code, Is.EqualTo("TW0301"));
            Assert.That(error.File, Is.EqualTo("src/main.lua"));
            Assert.That(error.Line, Is.EqualTo(1));
            Assert.That(error.Column, Is.GreaterThan(0));
        }

        [Test]
        public async Task InstallPutsTheScriptWhereTheScriptsTabFindsIt()
        {
            var project = await NewProject("my_hud");

            var result = await _twllink.BuildAsync(project, _game.Engine);

            Assert.That(result.Success, Is.True, result.ToolMessage);
            Assert.That(result.Build.Installed, Is.Not.Null);
            var script = GameScripts.Scan(_game.Engine, null).Single();
            Assert.That(script.FileName, Is.EqualTo("my_hud.lua"));
            Assert.That(script.Origin, Is.EqualTo(ScriptOrigin.User));
            Assert.That(script.HasMap, Is.True);
            Assert.That(script.Header.IsBundle, Is.True);
        }

        [Test]
        public async Task ABadCommandLineIsTextNotJson()
        {
            var notAProject = Path.Combine(_game.Root, "empty");
            Directory.CreateDirectory(notAProject);

            var result = await _twllink.BuildAsync(notAProject);

            Assert.That(result.ExitCode, Is.EqualTo(TwllinkResult.ExitUsage));
            Assert.That(result.Build, Is.Null);
            Assert.That(result.ToolMessage, Does.StartWith("twllink: no twproject.json"));
        }

        [Test]
        public async Task AProjectFolderThatIsGoneIsTwllinksAnswerNotAFailureToStart()
        {
            var result = await _twllink.BuildAsync(Path.Combine(_game.Root, "deleted", "my_hud"));

            Assert.That(result.ExitCode, Is.EqualTo(TwllinkResult.ExitUsage));
            Assert.That(result.ToolMessage, Does.Contain("does not exist"));
        }

        [Test]
        public async Task NewIntoAFolderInUseFailsWithTheReason()
        {
            var project = await NewProject("my_hud");

            var result = await _twllink.NewAsync("hud", project);

            Assert.That(result.ExitCode, Is.EqualTo(TwllinkResult.ExitUsage));
            Assert.That(result.ToolMessage, Does.StartWith("twllink: "));
        }

        [Test]
        public async Task AMissingExecutableIsAResultNotAnException()
        {
            var result = await new Twllink(Path.Combine(_game.Root, "nowhere", Twllink.ExecutableName)).BuildAsync(_game.Root);

            Assert.That(result.ExitCode, Is.EqualTo(-1));
            Assert.That(result.ToolMessage, Does.StartWith("Cannot run"));
        }

        [Test]
        public void AManifestThatDidNotReadLeavesTheIdOut()
        {
            var build = Twllink.ParseBuild("""
                { "twllink": "1.1", "success": false, "project": "C:\\p", "output": null, "map": null, "installed": null,
                  "modules": [], "diagnostics": [ { "severity": "error", "code": "TW0001", "file": "twproject.json",
                  "line": 0, "column": 0, "message": "bad" } ], "errors": 1, "warnings": 0 }
                """);

            Assert.That(build.Id, Is.Null);
            Assert.That(build.Output, Is.Null);
            Assert.That(build.Diagnostics.Single().Code, Is.EqualTo("TW0001"));
        }
    }
}
