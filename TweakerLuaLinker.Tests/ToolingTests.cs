using System;
using System.IO;
using System.Text.Json;

using NUnit.Framework;

using TweakerLuaLinker.Diagnostics;
using TweakerLuaLinker.Linking;
using TweakerLuaLinker.Project;

namespace TweakerLuaLinker.Tests
{
    /// <summary>The authoring side of the command line (Ф5b): --new, --install, --api.</summary>
    [TestFixture]
    public class ToolingTests
    {
        /// <summary>A scratch folder that is not a project, for --new and fake game installs.</summary>
        private sealed class Scratch : IDisposable
        {
            public string Root { get; } = Path.Combine(Path.GetTempPath(), "twllink-tests", Guid.NewGuid().ToString("N"));

            public Scratch()
            {
                Directory.CreateDirectory(Root);
            }

            public void Dispose()
            {
                try
                {
                    Directory.Delete(Root, true);
                }
                catch (IOException)
                {
                }
            }
        }

        private static (int Code, string Out, string Err) Run(string cwd, params string[] args)
        {
            var stdout = new StringWriter();
            var stderr = new StringWriter();
            var code = Cli.Run(args, stdout, stderr, cwd);
            return (code, stdout.ToString(), stderr.ToString());
        }

        [TestCase("hud")]
        [TestCase("hook")]
        [TestCase("empty")]
        public void NewMakesAProjectThatBuildsCleanly(string template)
        {
            using var scratch = new Scratch();
            var (code, output, error) = Run(scratch.Root, "--new", template, "My HUD!");
            Assert.That(code, Is.EqualTo(Cli.ExitSuccess), error);
            Assert.That(output, Does.Contain($"created a '{template}' project"));

            var project = Path.Combine(scratch.Root, "My HUD!");
            foreach (var file in new[] { "twproject.json", "src/main.lua", ".luarc.json", ".gitignore", ".twllink/tw.def.lua", ".twllink/twproject.schema.json" })
                Assert.That(File.Exists(Path.Combine(project, file)), Is.True, file);

            var result = Linker.Link(project);
            Assert.That(result.Success, Is.True, Assertions.Describe(result.Diagnostics));
            Assert.That(result.Diagnostics, Is.Empty, "a template is an example - it must not start with a warning");
            Assert.That(result.Manifest!.Id, Is.EqualTo("my_hud"));
            Assert.That(result.Manifest.Name, Is.EqualTo("My HUD!"));
            Assert.That(result.Manifest.Api, Is.EqualTo(TweakerLuaLinker.Api.ApiDescription.Embedded.Api));

            // The editor files are what this twllink carries, byte for byte.
            Assert.That(File.ReadAllText(Path.Combine(project, ".twllink", "tw.def.lua")), Is.EqualTo(Scaffold.Resource(Scaffold.DefinitionsFile)));
            using var luarc = JsonDocument.Parse(File.ReadAllText(Path.Combine(project, ".luarc.json")));
            Assert.That(luarc.RootElement.GetProperty("runtime.version").GetString(), Is.EqualTo("LuaJIT"));
        }

        [TestCase("hud")]
        [TestCase("hook")]
        public void TemplatesRunUnderLua(string template)
        {
            // Parsing is not running: each template's bundle is executed with a stub tw that records the
            // frame callback and calls it once.
            using var scratch = new Scratch();
            Run(scratch.Root, "--new", template, "t");
            var result = Linker.Link(Path.Combine(scratch.Root, "t"));
            // minilua has no math library; the game's LuaJIT does.
            const string stub = """
                local frame
                math = math or { abs = function(x) return x < 0 and -x or x end, max = function(a, b) return a > b and a or b end }
                tw = {
                    float_ch = function() return { get = function() return 1234567 end } end,
                    on_frame = function(fn) frame = fn end,
                    on_call = function() end,
                    theme = function() return 0 end,
                    hud = { safe = function() return 0, 0, 100, 100 end, text = function(x, y, s) io.write(s) end },
                }
                """;
            var run = LuaRunner.Run(stub + "\n" + result.Bundle + "\nframe()\n");
            Assert.That(run.ExitCode, Is.EqualTo(0), run.Output);
            Assert.That(run.Output, Is.EqualTo(template == "hud" ? "Score: 1 234 567" : "Blocks collected: 0"));
        }

        [Test]
        public void NewRefusesAFolderThatHasSomethingInIt()
        {
            using var scratch = new Scratch();
            File.WriteAllText(Path.Combine(scratch.Root, "notes.txt"), "mine");
            Assert.That(Run(scratch.Root, "--new", "hud", ".").Code, Is.EqualTo(Cli.ExitUsage));
            Assert.That(Run(scratch.Root, "--new", "fancy", "x").Err, Does.Contain("unknown template 'fancy'"));
        }

        [TestCase("My HUD!", "my_hud")]
        [TestCase("  __Weird--Name__ ", "weird_name")]
        [TestCase("Юникод", "my_script")]
        public void IdComesFromTheFolderName(string folder, string id)
        {
            Assert.That(Scaffold.IdFrom(folder), Is.EqualTo(id));
            Assert.That(ManifestReader.IsValidId(Scaffold.IdFrom(folder)), Is.True);
        }

        [Test]
        public void ABuildBringsTheEditorFilesUpToDate()
        {
            using var scratch = new Scratch();
            Run(scratch.Root, "--new", "empty", "p");
            var project = Path.Combine(scratch.Root, "p");
            var defs = Path.Combine(project, ".twllink", "tw.def.lua");
            File.WriteAllText(defs, "-- from an older Tweaker\n");

            Assert.That(Run(project).Code, Is.EqualTo(Cli.ExitSuccess));
            Assert.That(File.ReadAllText(defs), Is.EqualTo(Scaffold.Resource(Scaffold.DefinitionsFile)));

            // A project without .twllink/ is not given one.
            using var bare = new TestProject().Main("local x = 1\n");
            Run(bare.Root);
            Assert.That(Directory.Exists(Path.Combine(bare.Root, ".twllink")), Is.False);
        }

        [Test]
        public void InstallCopiesTheScriptAndItsMapIntoTheGame()
        {
            using var game = new Scratch();
            Directory.CreateDirectory(Path.Combine(game.Root, "engine", "TweakerStuff"));
            File.WriteAllText(Path.Combine(game.Root, "engine", "QuestViewer.exe"), "");
            using var project = new TestProject().Main("local x = 1\n");

            var (code, output, _) = Run(project.Root, "--install", "--game", game.Root);
            Assert.That(code, Is.EqualTo(Cli.ExitSuccess), output);
            var scripts = Path.Combine(game.Root, "engine", "TweakerStuff", "Scripts");
            Assert.That(File.ReadAllText(Path.Combine(scripts, "demo.lua")), Is.EqualTo(project.Link().Bundle));
            Assert.That(File.Exists(Path.Combine(scripts, "demo.lua.map")), Is.True);
            Assert.That(output, Does.Contain("installed: "));

            // The engine folder itself is accepted too, and --json says where it went.
            var (_, json, _) = Run(project.Root, "--install", "--game", Path.Combine(game.Root, "engine"), "--json");
            using var document = JsonDocument.Parse(json);
            Assert.That(document.RootElement.GetProperty("installed").GetString(), Is.EqualTo(Path.Combine(scripts, "demo.lua")));
        }

        [Test]
        public void InstallRefusesSomethingThatIsNotAGameWithThePlugin()
        {
            using var notGame = new Scratch();
            using var project = new TestProject().Main("local x = 1\n");
            var (code, output, _) = Run(project.Root, "--install", "--game", notGame.Root);
            Assert.That(code, Is.EqualTo(Cli.ExitBuildFailed));
            Assert.That(output, Does.Contain(DiagnosticCodes.InstallFailed).And.Contain("not an Audiosurf install"));
            Assert.That(File.Exists(Path.Combine(project.Root, "build", "demo.lua")), Is.True, "the build itself still happened");

            File.WriteAllText(Path.Combine(notGame.Root, "QuestViewer.exe"), "");
            Assert.That(Run(project.Root, "--install", "--game", notGame.Root).Out, Does.Contain("plugin is not installed"));
        }

        [Test]
        public void ApiOptionChecksAgainstAnotherDescription()
        {
            using var scratch = new Scratch();
            var api = Path.Combine(scratch.Root, "api.json");
            File.WriteAllText(api, """{ "format": 1, "api": 1, "globals": { "tw": { "kind": "table" } }, "types": {}, "removed": [] }""");
            using var project = new TestProject().Main("local h = tw.float_ch('A', 'b')\n");

            Assert.That(Run(project.Root).Code, Is.EqualTo(Cli.ExitSuccess));
            var (code, output, _) = Run(project.Root, "--api", api);
            Assert.That(code, Is.EqualTo(Cli.ExitBuildFailed));
            Assert.That(output, Does.Contain(DiagnosticCodes.UnknownTwName));
        }
    }
}
