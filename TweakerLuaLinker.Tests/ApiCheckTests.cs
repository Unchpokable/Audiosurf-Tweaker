using System.IO;
using System.Linq;
using System.Text;

using NUnit.Framework;

using TweakerLuaLinker.Api;
using TweakerLuaLinker.Diagnostics;
using TweakerLuaLinker.Linking;

using static TweakerLuaLinker.Tests.Assertions;

namespace TweakerLuaLinker.Tests
{
    /// <summary>The TW03xx checks: what the project's code reaches for, against api.json.</summary>
    [TestFixture]
    public class ApiCheckTests
    {
        private static LinkResult Link(string main, ApiDescription? api = null, string apiField = "1")
        {
            using var project = new TestProject().Field("api", apiField).Main(main);
            return Linker.Link(project.Root, api);
        }

        private static ApiDescription Tiny(int api, string extraGlobals = "")
        {
            var json = $$"""
                {
                  "format": 1,
                  "api": {{api}},
                  "globals": {
                    "string": { "kind": "table" },
                    "string.format": { "kind": "function" },
                    "tw": { "kind": "table" },
                    "tw.log": { "kind": "function", "params": ["msg"], "since": 1 }{{extraGlobals}}
                  },
                  "types": {},
                  "removed": ["io", "os.execute"]
                }
                """;
            return ApiDescription.Parse(new MemoryStream(Encoding.UTF8.GetBytes(json)));
        }

        [Test]
        public void TheEmbeddedDescriptionIsThePluginsApi()
        {
            var api = ApiDescription.Embedded;
            Assert.That(api.Api, Is.GreaterThanOrEqualTo(1));
            Assert.That(api.Globals["tw.float_ch"].Returns, Is.EqualTo("FloatHandle"));
            Assert.That(api.Globals["tw.hud.text"].Params, Is.EqualTo(new[] { "x", "y", "text", "color", "size", "font" }));
            Assert.That(api.Types.ContainsKey("FloatHandle:get"), Is.True);
            Assert.That(api.Removed, Does.Contain("io").And.Contain("os.execute"));
            Assert.That(api.Globals.ContainsKey("io"), Is.False, "what the sandbox removes is not a global");
        }

        [TestCase("local f = io.open('x')", "'io'")]
        [TestCase("os.execute('dir')", "'os.execute'")]
        [TestCase("local d = debug", "'debug'")]
        [TestCase("local l = loadstring('return 1')", "'loadstring'")]
        public void SandboxedNamesAreErrors(string main, string name)
        {
            var d = Single(Link(main), DiagnosticCodes.SandboxRemoved);
            Assert.That(d.Message, Does.Contain(name));
        }

        [Test]
        public void TheBundlesOwnRequireIsNotTheSandboxedOne()
        {
            using var project = new TestProject().Main("local a = require('a')\n").Write("src/a.lua", "return 1\n");
            var result = project.Link();
            Succeeded(result);
            Assert.That(result.Diagnostics, Is.Empty);
        }

        [TestCase("local h = tw.float_chh('A', 'b')", "'tw.float_chh'", "did you mean 'tw.float_ch'")]
        [TestCase("tw.hud.txt(1, 2, 'x')", "'tw.hud.txt'", "did you mean 'tw.hud.text'")]
        [TestCase("local c = tw.hud.corners.middle", "'tw.hud.corners.middle'", "")]
        [TestCase("tw.zzzzzzzz()", "'tw.zzzzzzzz'", "")]
        public void UnknownTwNamesAreErrorsWithASuggestion(string main, string name, string hint)
        {
            var d = Single(Link(main), DiagnosticCodes.UnknownTwName);
            Assert.That(d.Message, Does.Contain(name).And.Contain(hint));
            if (hint.Length == 0)
                Assert.That(d.Message, Does.Not.Contain("did you mean"), "no suggestion when nothing is close");
        }

        [Test]
        public void KnownNamesPass()
        {
            var result = Link("""
                local h = tw.float_ch("StatCollector", "Points")
                local c = tw.hud.corners.top_left
                local v = tw.api_version + os.clock() + math.floor(1.5)
                print(string.format("%d", v), bit.band(1, 3), tw.channel)
                tw.on_frame(function() tw.hud.text(1, 2, "x", tw.theme("text_primary")) end)
                local s = ("x"):rep(3)
                _G.shared = 1
                local u = _G.anything
                """);
            Succeeded(result);
            Assert.That(result.Diagnostics, Is.Empty, Describe(result.Diagnostics));
        }

        [Test]
        public void WritingIntoTwIsAnError()
        {
            var result = Link("tw.mine = 1\nfunction tw.helper() end\nfunction tw.hud.box() end\ntw = {}\n");
            var lines = result.Diagnostics.Where(d => d.Code == DiagnosticCodes.WritesTw).Select(d => d.Line);
            Assert.That(lines, Is.EqualTo(new[] { 1, 2, 3, 4 }));
        }

        [Test]
        public void AGlobalAssignmentIsAWarningOncePerName()
        {
            var result = Link("leaked = 1\nleaked = 2\nfunction helper() end\n");
            Succeeded(result);
            var names = result.Diagnostics.Where(d => d.Code == DiagnosticCodes.GlobalAssigned).Select(d => d.Line);
            Assert.That(names, Is.EqualTo(new[] { 1, 3 }));
        }

        [Test]
        public void LocalsAndFieldsOfLocalsAreNotGlobals()
        {
            var result = Link("local t = {}\nfunction t.method() end\nfunction t:other() end\nlocal function inner() end\nlocal x\nx = 1\n");
            Succeeded(result);
            Assert.That(result.Diagnostics, Is.Empty, Describe(result.Diagnostics));
        }

        [Test]
        public void AnUnknownGlobalIsAWarningUnlessSomeModuleAssignsIt()
        {
            var unknown = Link("local y = mathh.floor(1.5)\nlocal z = mathh.ceil(1.5)\n");
            Succeeded(unknown);
            Assert.That(Single(unknown, DiagnosticCodes.UnknownGlobal).Message, Does.Contain("'mathh'"));

            using var project = new TestProject()
                .Main("local config = require('config')\nlocal v = SETTINGS.speed\n")
                .Write("src/config.lua", "SETTINGS = { speed = 1 }\nreturn true\n");
            var shared = project.Link();
            Succeeded(shared);
            Assert.That(shared.Diagnostics.Select(d => d.Code), Is.EqualTo(new[] { DiagnosticCodes.GlobalAssigned }),
                "assigned in one module, read in another: the assignment is worth a warning, the read is not");
        }

        [Test]
        public void AMissingLibraryFieldIsAWarning()
        {
            var result = Link("local z = string.formatt('%d', 1)");
            Succeeded(result);
            Assert.That(Single(result, DiagnosticCodes.UnknownLibraryName).Message, Does.Contain("did you mean 'string.format'"));
        }

        [Test]
        public void ANameNewerThanTheTargetIsAWarning()
        {
            var api = Tiny(2, ",\n    \"tw.shiny\": { \"kind\": \"function\", \"params\": [], \"since\": 2 }");
            var old = Link("tw.log('x')\ntw.shiny()\n", api, "1");
            Succeeded(old);
            var d = Single(old, DiagnosticCodes.NewerThanTarget);
            Assert.That((d.Line, d.Message.Contains("API 2")), Is.EqualTo((2, true)));

            Assert.That(Link("tw.shiny()\n", api, "2").Diagnostics, Is.Empty, "a project targeting API 2 may use it");
        }

        [Test]
        public void AProjectForANewerApiThanTheLinkerKnowsIsTW0016()
        {
            var result = Link("tw.log('x')\n", Tiny(1), "2");
            Assert.That(result.Success, Is.False);
            Single(result, DiagnosticCodes.ApiTooNew);
        }

        [Test]
        public void ModulesWithSyntaxErrorsAreNotChecked()
        {
            var result = Link("local x = (\nio.open('x')\n");
            Assert.That(result.Diagnostics.Select(d => d.Code).Distinct(), Is.EqualTo(new[] { DiagnosticCodes.SyntaxError }));
        }
    }
}
