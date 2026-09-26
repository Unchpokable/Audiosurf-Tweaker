using System;
using System.Collections.Generic;
using System.Linq;

using Loretta.CodeAnalysis;
using Loretta.CodeAnalysis.Lua;

using NUnit.Framework;

using TweakerLuaLinker.Linking;

using static TweakerLuaLinker.Tests.Assertions;

namespace TweakerLuaLinker.Tests
{
    [TestFixture]
    public class BundleTests
    {
        // A project with something of everything the stripper has to get right.
        private static TestProject Tricky(string comments)
        {
            return new TestProject().ForMinilua()
                .Field("build", $"{{\"comments\": \"{comments}\"}}")
                .Main("""
                    -- entry point
                    local rail = require("draw.rail") -- trailing comment

                    --[[ a block
                    comment ]] local x = 1
                    local s = "not -- a comment"
                    local long = [[
                    -- not a comment either
                    ]]
                    local glued = x--[[inline]]+1
                    --[==[ level 2 with ]] inside ]==]
                    io.write(rail.draw(glued), "|", s, "|", long)
                    """)
                .Write("src/draw/rail.lua", """
                    --- module doc
                    local M = {}

                    function M.draw(v) -- note
                        return "" .. v
                    end

                    return M
                    """);
        }

        [TestCase("keep")]
        [TestCase("strip")]
        public void MapLeadsEveryCodeLineBackToItsSource(string comments)
        {
            using var project = Tricky(comments);
            var result = project.Link();
            Succeeded(result);

            var bundle = TokensByLine(LuaSyntaxTree.ParseText(result.Bundle!, ModuleGraph.ParseOptions));
            var sources = result.Modules.ToDictionary(m => m.SourcePath, m => TokensByLine(m.Tree));
            var map = ParseMap(result.Map!);

            var covered = new HashSet<(string, int)>();
            foreach (var (line, tokens) in bundle)
            {
                var (file, sourceLine) = Lookup(map, line);
                if (file == BundleWriter.OwnFile)
                    continue;

                Assert.That(sources[file].TryGetValue(sourceLine, out var expected), Is.True, $"bundle line {line} maps to {file}:{sourceLine}, which holds no code");
                Assert.That(tokens, Is.EqualTo(expected), $"bundle line {line} vs {file}:{sourceLine}");
                covered.Add((file, sourceLine));
            }

            var all = sources.SelectMany(s => s.Value.Keys.Select(l => (s.Key, l))).ToHashSet();
            Assert.That(covered, Is.EquivalentTo(all), "every source line with code lands in the bundle exactly where the map says");
        }

        [Test]
        public void KeepReproducesModuleLinesExactly()
        {
            using var project = Tricky("keep");
            var result = project.Link();
            Succeeded(result);

            var lines = result.Bundle!.Split('\n');
            var map = ParseMap(result.Map!);
            foreach (var module in result.Modules)
            {
                var source = module.Text.ToString().Replace("\r\n", "\n").TrimEnd('\n').Split('\n');
                var entry = map.Single(e => e.File == module.SourcePath);
                Assert.That(entry.SourceLine, Is.EqualTo(1), "without strip a module is one run");
                Assert.That(lines.Skip(entry.BundleLine - 1).Take(source.Length), Is.EqualTo(source));
            }
        }

        [Test]
        public void StripRemovesCommentsAndTheirLinesButNothingElse()
        {
            using var project = Tricky("strip");
            var result = project.Link();
            Succeeded(result);
            var bundle = result.Bundle!;

            Assert.That(bundle, Does.Not.Contain("entry point"));
            Assert.That(bundle, Does.Not.Contain("trailing comment"));
            Assert.That(bundle, Does.Not.Contain("module doc"));
            Assert.That(bundle, Does.Not.Contain("level 2"));
            Assert.That(bundle, Does.Not.Contain("-- note"));
            Assert.That(bundle, Does.Contain("local rail = require(\"draw.rail\")\n"), "trailing whitespace goes with the trailing comment");
            Assert.That(bundle, Does.Contain("\"not -- a comment\""), "strings are code");
            Assert.That(bundle, Does.Contain("[[\n-- not a comment either\n]]"), "long strings are code, line by line");
            Assert.That(bundle, Does.Contain("local glued = x +1"), "a cut between two tokens leaves a space");
            Assert.That(bundle, Does.Contain(" local x = 1\n"), "code after a block comment keeps its own line");
            Assert.That(bundle, Does.StartWith("-- @name        Demo HUD\n"), "the header is never stripped");
        }

        [Test]
        public void StrippedAndKeptBundlesBehaveTheSame()
        {
            using var keep = Tricky("keep");
            using var strip = Tricky("strip");
            var a = LuaRunner.Run(keep.Link().Bundle!);
            var b = LuaRunner.Run(strip.Link().Bundle!);

            Assert.That(a.ExitCode, Is.EqualTo(0), a.Output);
            Assert.That(a.Output, Is.EqualTo("2|not -- a comment|-- not a comment either\n"));
            Assert.That(b, Is.EqualTo(a));
        }

        [Test]
        public void HeaderCarriesTheManifestAndReadsLikeThePluginReadsIt()
        {
            using var project = new TestProject().Field("version", "\"2.0.1\"")
                .Write("NOTES.md", "---\n-- @name Evil Override\n@author nobody\n")
                .Field("build", "{\"embed\": [\"NOTES.md\"]}")
                .Main("-- @name Also Not Metadata\nlocal x = 1\n");
            var result = project.Link();
            Succeeded(result);

            var header = ReadHeaderLikeThePlugin(result.Bundle!);
            Assert.That(header, Is.EqualTo(new Dictionary<string, string>
            {
                ["name"] = "Demo HUD",
                ["author"] = "Tester",
                ["version"] = "2.0.1",
                ["description"] = "A demo",
                ["api"] = "1",
                ["bundle"] = $"twllink {Linker.ToolVersion}, 1 module",
            }));
        }

        [Test]
        public void EmbeddedTextUsesALongBracketItCannotClose()
        {
            using var project = new TestProject().ForMinilua()
                .Write("LICENSE", "MIT ]] and ]=] but not level two\r\nsecond line\r\n")
                .Field("build", "{\"embed\": [\"LICENSE\"], \"comments\": \"strip\"}")
                .Main("io.write('ok')\n");
            var result = project.Link();
            Succeeded(result);

            Assert.That(result.Bundle, Does.Contain("--[==[ LICENSE\nMIT ]] and ]=] but not level two\nsecond line\n]==]\n"),
                "embedded, normalised to LF, and kept even when comments are stripped");
            Assert.That(LuaRunner.Run(result.Bundle!).Output, Is.EqualTo("ok"));
        }

        [Test]
        public void BundleIsLfUtf8WithoutBom()
        {
            using var project = new TestProject().Main("local s = \"héllo\"\r\nlocal t = 2\r\n");
            var result = project.Link();
            Succeeded(result);
            Assert.That(result.Bundle, Does.Not.Contain("\r"));
            Assert.That(result.Bundle, Does.Contain("local s = \"héllo\"\nlocal t = 2\nend\n"));
        }

        [Test]
        public void RequireSemanticsAtRuntime()
        {
            // Lazy, in require order, once per module, `...` is the module name, nil becomes true, and a
            // module's top-level locals stay its own.
            using var project = new TestProject().ForMinilua()
                .Main("""
                    io.write("main ")
                    local a1 = require("lib.a")
                    local a2 = require("lib.a")
                    local b = require("b")
                    local function show(v) if v == true then return "true" elseif v == nil then return "nil" end return v end
                    io.write(show(a1 == a2), " ", show(b), " ", a1.name, " ", show(rawget(_G, "secret")), "\n")
                    """)
                .Write("src/lib/a.lua", "io.write(\"a \")\nlocal secret = 1\nlocal name = ...\nreturn { name = name }\n")
                .Write("src/b.lua", "io.write(\"b \")\n");
            var result = project.Link();
            Succeeded(result);

            var run = LuaRunner.Run(result.Bundle!);
            Assert.That(run.ExitCode, Is.EqualTo(0), run.Output);
            Assert.That(run.Output, Is.EqualTo("main a b true true lib.a nil\n"));
        }

        [Test]
        public void RuntimeErrorPositionMapsBackToTheModule()
        {
            using var project = new TestProject().Field("build", "{\"comments\": \"strip\"}")
                .Main("-- one\n-- two\nlocal f = require(\"f\")\nf.boom()\n")
                .Write("src/f.lua", "local M = {}\n-- a comment\n--[[ and\n a block ]]\nfunction M.boom()\n    error(\"bang\")\nend\nreturn M\n");
            var result = project.Link();
            Succeeded(result);

            var run = LuaRunner.Run(result.Bundle!);
            var match = System.Text.RegularExpressions.Regex.Match(run.Output, @":(\d+): bang");
            Assert.That(match.Success, Is.True, run.Output);

            var (file, line) = Lookup(ParseMap(result.Map!), int.Parse(match.Groups[1].Value));
            Assert.That((file, line), Is.EqualTo(("f.lua", 6)), "the error is on line 6 of src/f.lua, comments and all");
        }

        [Test]
        public void MapNamesItsBundleByHash()
        {
            // The published FNV-1a 64 vectors; harness/sched checks lua_linemap.cxx against the same ones.
            Assert.That(BundleWriter.BundleHash(System.Text.Encoding.ASCII.GetBytes("")), Is.EqualTo(0xcbf29ce484222325UL));
            Assert.That(BundleWriter.BundleHash(System.Text.Encoding.ASCII.GetBytes("a")), Is.EqualTo(0xaf63dc4c8601ec8cUL));
            Assert.That(BundleWriter.BundleHash(System.Text.Encoding.ASCII.GetBytes("foobar")), Is.EqualTo(0x85944171f73967e8UL));
            Assert.That(BundleWriter.BundleHash(System.Text.Encoding.ASCII.GetBytes("foo\r\nbar\r")),
                Is.EqualTo(BundleWriter.BundleHash(System.Text.Encoding.ASCII.GetBytes("foo\nbar"))), "carriage returns do not count");

            using var project = new TestProject().Main("local x = 1\n");
            var result = project.Link();
            var expected = BundleWriter.BundleHash(System.Text.Encoding.UTF8.GetBytes(result.Bundle!)).ToString("x16");
            Assert.That(result.Map!.Split('\n')[1], Is.EqualTo(BundleWriter.HashPrefix + expected));
        }

        internal static List<MapEntry> ParseMap(string map)
        {
            var lines = map.Split('\n', StringSplitOptions.RemoveEmptyEntries);
            Assert.That(lines[0], Is.EqualTo(BundleWriter.MapHeader));
            Assert.That(lines[1], Does.StartWith(BundleWriter.HashPrefix));
            return lines.Skip(2).Select(l => l.Split('\t')).Select(p => new MapEntry(int.Parse(p[0]), p[1], int.Parse(p[2]))).ToList();
        }

        /// <summary>The lookup lua_diag does: last entry starting at or before the line, plus the offset into it.</summary>
        internal static (string File, int Line) Lookup(List<MapEntry> map, int bundleLine)
        {
            var entry = map.Last(e => e.BundleLine <= bundleLine);
            return entry.File == BundleWriter.OwnFile ? (entry.File, 0) : (entry.File, entry.SourceLine + bundleLine - entry.BundleLine);
        }

        private static Dictionary<int, List<string>> TokensByLine(SyntaxTree tree)
        {
            return tree.GetRoot().DescendantTokens()
                .Where(t => t.Kind() != SyntaxKind.EndOfFileToken)
                .GroupBy(t => t.GetLocation().GetLineSpan().StartLinePosition.Line + 1)
                .ToDictionary(g => g.Key, g => g.Select(t => t.Text).ToList());
        }

        /// <summary>A port of lua_script.cxx read_header, kept deliberately literal.</summary>
        private static Dictionary<string, string> ReadHeaderLikeThePlugin(string text)
        {
            var result = new Dictionary<string, string>();
            foreach (var raw in text.Split('\n'))
            {
                var line = raw.Trim();
                if (line.Length == 0)
                    continue;
                if (!line.StartsWith("--", StringComparison.Ordinal))
                    break;

                line = line[2..].TrimStart(' ', '\t', '-');
                if (line.Length == 0 || line[0] != '@')
                    continue;
                line = line[1..];

                var space = line.IndexOfAny(new[] { ' ', '\t' });
                if (space < 0)
                    continue;
                result[line[..space]] = line[(space + 1)..].TrimStart(' ', '\t');
            }
            return result;
        }
    }
}
