using System.Linq;

using NUnit.Framework;

using TweakerLuaLinker.Diagnostics;

using static TweakerLuaLinker.Tests.Assertions;

namespace TweakerLuaLinker.Tests
{
    [TestFixture]
    public class GraphTests
    {
        [Test]
        public void MissingMainIsTW0101()
        {
            using var project = new TestProject().Write("src/init.lua", "local x = 1\n");
            var d = Single(project.Link(), DiagnosticCodes.MainMissing);
            Assert.That(d.File, Is.EqualTo("src/main.lua"));
        }

        [Test]
        public void SyntaxErrorIsTW0102AtItsPosition()
        {
            using var project = new TestProject()
                .Main("local a = require(\"a\")\n")
                .Write("src/a.lua", "local ok = 1\nlocal broken = (2 +\nreturn {}\n");
            var result = project.Link();

            Assert.That(result.Success, Is.False);
            var errors = result.Diagnostics.Where(d => d.Code == DiagnosticCodes.SyntaxError).ToList();
            Assert.That(errors, Is.Not.Empty);
            Assert.That(errors[0].File, Is.EqualTo("src/a.lua"));
            Assert.That(errors[0].Line, Is.InRange(2, 3));
            Assert.That(errors[0].Message, Does.Contain("LUA"), "Loretta's own code is kept in the message");
        }

        [Test]
        public void LuaJitSyntaxIsAccepted()
        {
            using var project = new TestProject().Main("local m = 0xFFULL\nfor i = 1, 2 do\n  if i == 1 then goto continue end\n  ::continue::\nend\n");
            Succeeded(project.Link());
        }

        [TestCase("local name = \"a\"\nrequire(name)\n", 2)]
        [TestCase("require(\"a\" .. \"b\")\n", 1)]
        [TestCase("require()\n", 1)]
        [TestCase("require(\"a\", \"b\")\n", 1)]
        [TestCase("require { \"a\" }\n", 1)]
        public void NonLiteralRequireIsTW0103(string main, int line)
        {
            using var project = new TestProject().Main(main).Write("src/a.lua", "").Write("src/ab.lua", "");
            Assert.That(Single(project.Link(), DiagnosticCodes.DynamicRequire).Line, Is.EqualTo(line));
        }

        [Test]
        public void BothCallFormsAreFound()
        {
            using var project = new TestProject()
                .Main("local a = require(\"a\")\nlocal b = require \"b\"\nlocal c = require [[c]]\n")
                .Write("src/a.lua", "").Write("src/b.lua", "").Write("src/c.lua", "");
            var result = project.Link();
            Succeeded(result);
            Assert.That(result.Modules.Select(m => m.Name), Is.EqualTo(new[] { "a", "b", "c", "main" }));
        }

        [Test]
        public void RequireInCommentsAndStringsIsNotARequire()
        {
            using var project = new TestProject().Main("-- require(\"ghost\")\nlocal s = \"require('ghost')\"\n--[[ require \"ghost\" ]]\n");
            var result = project.Link();
            Succeeded(result);
            Assert.That(result.Modules.Select(m => m.Name), Is.EqualTo(new[] { "main" }));
        }

        [Test]
        public void ALocalRequireIsNotOurs()
        {
            // The module brings its own require; the linker must neither resolve nor reject its argument.
            using var project = new TestProject().Main("local require = function(x) return x end\nlocal v = require(\"not.a.module\")\nlocal w = require(v)\n");
            Succeeded(project.Link());
        }

        [Test]
        public void MissingModuleIsTW0104WithTheExpectedPath()
        {
            using var project = new TestProject().Main("local x = 1\nlocal r = require(\"draw.rail\")\n");
            var d = Single(project.Link(), DiagnosticCodes.ModuleNotFound);
            Assert.That((d.File, d.Line, d.Column), Is.EqualTo(("src/main.lua", 2, 11)));
            Assert.That(d.Message, Does.Contain("src/draw/rail.lua"));
        }

        [TestCase("draw/rail")]
        [TestCase("draw..rail")]
        [TestCase(".rail")]
        [TestCase("draw.rail.lua.")]
        [TestCase("dra w")]
        public void BadModuleNameIsTW0105(string name)
        {
            using var project = new TestProject().Main($"local r = require(\"{name}\")\n");
            Single(project.Link(), DiagnosticCodes.ModuleNameInvalid);
        }

        [TestCase("Palette")]
        [TestCase("Draw.rail")]
        public void CaseMismatchIsTW0106(string name)
        {
            using var project = new TestProject().Main($"local p = require(\"{name}\")\n")
                .Write("src/palette.lua", "return {}\n").Write("src/draw/rail.lua", "return {}\n");
            Single(project.Link(), DiagnosticCodes.ModuleNameCase);
        }

        [Test]
        public void CycleIsTW0107WithThePath()
        {
            using var project = new TestProject().Main("local a = require(\"a\")\n")
                .Write("src/a.lua", "local b = require(\"b\")\nreturn {}\n")
                .Write("src/b.lua", "local c = require(\"c\")\nreturn {}\n")
                .Write("src/c.lua", "local a = require(\"a\")\nreturn {}\n");
            var d = Single(project.Link(), DiagnosticCodes.RequireCycle);
            Assert.That(d.Message, Does.Contain("a -> b -> c -> a"));
            Assert.That(d.File, Is.EqualTo("src/c.lua"));
        }

        [Test]
        public void SelfRequireIsACycle()
        {
            using var project = new TestProject().Main("local a = require(\"a\")\n").Write("src/a.lua", "local me = require(\"a\")\n");
            Single(project.Link(), DiagnosticCodes.RequireCycle);
        }

        [Test]
        public void RequiringMainIsTW0108()
        {
            using var project = new TestProject().Main("local a = require(\"a\")\n").Write("src/a.lua", "local m = require(\"main\")\n");
            Single(project.Link(), DiagnosticCodes.RequireMain);
        }

        [Test]
        public void UnreachableModuleIsAWarning()
        {
            using var project = new TestProject().Main("local a = require(\"a\")\n")
                .Write("src/a.lua", "").Write("src/lib/unused.lua", "").Write("docs/notes.lua", "");
            var result = project.Link();

            Succeeded(result);
            var d = Single(result, DiagnosticCodes.UnreachableModule);
            Assert.That(d.File, Is.EqualTo("src/lib/unused.lua"), "only src/ is the module root; docs/ is not looked at");
            Assert.That(d.Message, Does.Contain("'lib.unused'"));
        }

        [Test]
        public void InvalidUtf8IsTW0110()
        {
            using var project = new TestProject().Main("local a = require(\"a\")\n")
                .WriteBytes("src/a.lua", new byte[] { (byte)'-', (byte)'-', 0xC3, 0x28, (byte)'\n' });
            Single(project.Link(), DiagnosticCodes.SourceUnreadable);
        }

        [Test]
        public void OrderIsDependenciesFirstMainLastAndStable()
        {
            using var project = new TestProject()
                .Main("local ui = require(\"ui\")\nlocal pal = require(\"palette\")\n")
                .Write("src/ui.lua", "local p = require(\"palette\")\nlocal s = require(\"util.strings\")\nreturn {}\n")
                .Write("src/palette.lua", "local s = require(\"util.strings\")\nreturn {}\n")
                .Write("src/util/strings.lua", "return {}\n");

            var first = project.Link();
            Succeeded(first);
            Assert.That(first.Modules.Select(m => m.Name), Is.EqualTo(new[] { "util.strings", "palette", "ui", "main" }));
            Assert.That(project.Link().Bundle, Is.EqualTo(first.Bundle), "two builds of the same project are byte-identical");
        }

        [Test]
        public void EveryModuleIsIncludedOnceHoweverOftenItIsRequired()
        {
            using var project = new TestProject()
                .Main("local a = require(\"a\")\nlocal b = require(\"b\")\nlocal a2 = require(\"a\")\n")
                .Write("src/a.lua", "return {}\n").Write("src/b.lua", "local a = require(\"a\")\nreturn {}\n");
            var result = project.Link();
            Succeeded(result);
            Assert.That(result.Modules.Select(m => m.Name), Is.EqualTo(new[] { "a", "b", "main" }));
        }
    }
}
