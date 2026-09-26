using System;
using System.IO;
using System.Linq;
using System.Text.Json;

using NUnit.Framework;

namespace TweakerLuaLinker.Tests
{
    [TestFixture]
    public class CliTests
    {
        private static (int Code, string Out, string Err) Run(string cwd, params string[] args)
        {
            var stdout = new StringWriter();
            var stderr = new StringWriter();
            var code = Cli.Run(args, stdout, stderr, cwd);
            return (code, stdout.ToString(), stderr.ToString());
        }

        private static TestProject Broken()
        {
            // One error and one warning, in two files, so ordering and both severities are exercised.
            return new TestProject()
                .Main("local a = require(\"a\")\nlocal m = require(\"missing\")\n")
                .Write("src/a.lua", "return {}\n")
                .Write("src/spare.lua", "");
        }

        [Test]
        public void BuildsTheProjectInTheCurrentFolder()
        {
            using var project = new TestProject().Main("local x = 1\n");
            var (code, output, _) = Run(project.Root);

            Assert.That(code, Is.EqualTo(Cli.ExitSuccess), output);
            Assert.That(File.Exists(Path.Combine(project.Root, "build", "demo.lua")), Is.True);
            Assert.That(File.Exists(Path.Combine(project.Root, "build", "demo.lua.map")), Is.True);
            Assert.That(output, Does.Contain("twllink: demo 1.2.3 -> build/demo.lua (1 module,"));
        }

        [Test]
        public void ProjectMayBeTheFolderOrTheManifest()
        {
            using var project = new TestProject().Main("local x = 1\n");
            var elsewhere = Path.GetTempPath();
            Assert.That(Run(elsewhere, "--project", project.Root).Code, Is.EqualTo(Cli.ExitSuccess));
            Assert.That(Run(elsewhere, "--project", Path.Combine(project.Root, "twproject.json")).Code, Is.EqualTo(Cli.ExitSuccess));
        }

        [Test]
        public void OutMayBeAFileOrAFolder()
        {
            using var project = new TestProject().Main("local x = 1\n");
            Assert.That(Run(project.Root, "--out", "dist/custom.lua").Code, Is.EqualTo(Cli.ExitSuccess));
            Assert.That(File.Exists(Path.Combine(project.Root, "dist", "custom.lua.map")), Is.True);

            Assert.That(Run(project.Root, "--out", "scripts/").Code, Is.EqualTo(Cli.ExitSuccess));
            Assert.That(File.Exists(Path.Combine(project.Root, "scripts", "demo.lua")), Is.True);
        }

        [Test]
        public void UnchangedOutputIsNotRewritten()
        {
            using var project = new TestProject().Main("local x = 1\n");
            Run(project.Root);
            var file = Path.Combine(project.Root, "build", "demo.lua");
            var old = new DateTime(2020, 1, 1, 0, 0, 0, DateTimeKind.Utc);
            File.SetLastWriteTimeUtc(file, old);

            Run(project.Root);
            Assert.That(File.GetLastWriteTimeUtc(file), Is.EqualTo(old));

            project.Main("local x = 2\n");
            Run(project.Root);
            Assert.That(File.GetLastWriteTimeUtc(file), Is.Not.EqualTo(old));
        }

        [Test]
        public void ProjectErrorsExitOneAndWriteNothing()
        {
            using var project = Broken();
            var (code, output, _) = Run(project.Root);

            Assert.That(code, Is.EqualTo(Cli.ExitBuildFailed));
            Assert.That(output, Does.Contain("src/main.lua(2,11): error TW0104: module 'missing' not found"));
            Assert.That(output, Does.Contain("src/spare.lua: warning TW0109:"));
            Assert.That(output, Does.Contain("build failed - 1 error, 1 warning"));
            Assert.That(Directory.Exists(Path.Combine(project.Root, "build")), Is.False);
        }

        [TestCase("--frobnicate")]
        [TestCase("--project")]
        [TestCase("--api")]
        [TestCase("--game", "somewhere")]
        [TestCase("--new", "hud")]
        [TestCase("--new", "hud", "dir", "--watch")]
        [TestCase("--api", "no-such-api.json")]
        public void BadCommandLineExitsTwo(params string[] arguments)
        {
            using var project = new TestProject().Main("local x = 1\n");
            var (code, _, error) = Run(project.Root, arguments);
            Assert.That(code, Is.EqualTo(Cli.ExitUsage));
            Assert.That(error, Does.StartWith("twllink: "));
        }

        [Test]
        public void NoProjectHereExitsTwo()
        {
            var empty = Path.Combine(Path.GetTempPath(), "twllink-tests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(empty);
            try
            {
                var (code, _, error) = Run(empty);
                Assert.That(code, Is.EqualTo(Cli.ExitUsage));
                Assert.That(error, Does.Contain("no twproject.json in the current folder"));

                File.WriteAllText(Path.Combine(empty, "project.json"), "{}");
                Assert.That(Run(empty, "--project", "project.json").Code, Is.EqualTo(Cli.ExitUsage));
            }
            finally
            {
                Directory.Delete(empty, true);
            }
        }

        [Test]
        public void HelpAndVersionExitZero()
        {
            Assert.That(Run(Path.GetTempPath(), "--help").Out, Does.Contain("usage: twllink"));
            Assert.That(Run(Path.GetTempPath(), "--version").Out.Trim(), Is.EqualTo("twllink " + Linking.Linker.ToolVersion));
        }

        [Test]
        public void JsonCarriesExactlyWhatTheTextSays()
        {
            using var project = Broken();
            var human = Run(project.Root);
            var json = Run(project.Root, "--json");
            Assert.That(json.Code, Is.EqualTo(human.Code));

            using var document = JsonDocument.Parse(json.Out);
            var root = document.RootElement;
            Assert.That(root.GetProperty("success").GetBoolean(), Is.False);
            Assert.That(root.GetProperty("output").ValueKind, Is.EqualTo(JsonValueKind.Null));
            Assert.That(root.GetProperty("errors").GetInt32(), Is.EqualTo(1));
            Assert.That(root.GetProperty("warnings").GetInt32(), Is.EqualTo(1));

            // Rebuild the text lines from the JSON: the two views must be one list.
            var fromJson = root.GetProperty("diagnostics").EnumerateArray().Select(d =>
            {
                var line = d.GetProperty("line").GetInt32();
                var position = line > 0 ? $"({line},{d.GetProperty("column").GetInt32()})" : "";
                return $"{d.GetProperty("file").GetString()}{position}: {d.GetProperty("severity").GetString()} " +
                       $"{d.GetProperty("code").GetString()}: {d.GetProperty("message").GetString()}";
            }).ToList();
            var fromText = human.Out.Split(Environment.NewLine).Where(l => l.Contains(": error TW") || l.Contains(": warning TW")).ToList();
            Assert.That(fromJson, Is.EqualTo(fromText));
        }

        [Test]
        public void JsonOnSuccessNamesTheFilesAndModules()
        {
            using var project = new TestProject().Main("local a = require(\"a\")\n").Write("src/a.lua", "return 1\n");
            var (code, output, _) = Run(project.Root, "--json");
            Assert.That(code, Is.EqualTo(Cli.ExitSuccess));

            using var document = JsonDocument.Parse(output);
            var root = document.RootElement;
            Assert.That(root.GetProperty("success").GetBoolean(), Is.True);
            Assert.That(root.GetProperty("id").GetString(), Is.EqualTo("demo"));
            Assert.That(root.GetProperty("output").GetString(), Is.EqualTo(Path.Combine(project.Root, "build", "demo.lua")));
            Assert.That(root.GetProperty("map").GetString(), Is.EqualTo(Path.Combine(project.Root, "build", "demo.lua.map")));
            var modules = root.GetProperty("modules").EnumerateArray().Select(m => m.GetProperty("file").GetString()).ToList();
            Assert.That(modules, Is.EqualTo(new[] { "src/a.lua", "src/main.lua" }));
        }
    }
}
