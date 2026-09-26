using System;
using System.IO;
using System.Linq;

using NUnit.Framework;

using TweakerLuaLinker.Api;

namespace TweakerLuaLinker.Tests
{
    /// <summary>
    /// The three lists of names that must agree (lua-engine-fix-roadmap.md §9.8): the prelude, as
    /// Docs/scripting/api.json describes it (harness/lua keeps that one honest); the reference authors read,
    /// Docs/scripting/api-reference.md; and the editor definitions, Docs/scripting/tw.def.lua, generated
    /// from the other two. CLAUDE.md's "update api-reference.md when tw.* changes" stops being discipline.
    /// </summary>
    [TestFixture]
    public class ApiDocsTests
    {
        private static string Docs(string file)
        {
            var root = LuaRunner.RepositoryRoot() ?? throw new InconclusiveException("repository not found");
            return Path.Combine(root, "Docs", "scripting", file);
        }

        private static ApiDescription Api => ApiDescription.Load(Docs("api.json"));

        private static ReferenceEntry[] Reference => ApiReference.Parse(File.ReadAllText(Docs("api-reference.md"))).ToArray();

        [Test]
        public void TheEmbeddedApiIsTheCommittedOne()
        {
            Assert.That(ApiDescription.Embedded.Globals.Keys, Is.EquivalentTo(Api.Globals.Keys),
                "twllink was built with a different api.json than Docs/scripting/api.json - rebuild it");
        }

        [Test]
        public void EveryNameScriptsCanReachIsDocumented()
        {
            var documented = Reference.Select(r => r.Name).ToHashSet(StringComparer.Ordinal);
            var api = Api;

            var missing = api.Globals
                .Where(g => g.Key.StartsWith("tw.", StringComparison.Ordinal))
                .Where(g => !(g.Value.IsTable && api.Globals.Any(o => o.Key.StartsWith(g.Key + ".", StringComparison.Ordinal) && o.Value.IsFunction)))
                .Where(g => !IsFieldOfDocumentedTable(g.Key, documented))
                .Select(g => g.Key)
                .Where(name => !documented.Contains(name))
                .ToList();

            foreach (var method in api.Types.Keys)
            {
                var (type, name) = (method[..method.IndexOf(':')], method[(method.IndexOf(':') + 1)..]);
                var receivers = new[] { "handle", "array", "mute" }.Where(r => ApiReference.TypesOf(r).Contains(type));
                if (!receivers.Any(r => documented.Contains(r + ":" + name)))
                    missing.Add(method);
            }

            Assert.That(missing, Is.Empty, "in the plugin's API but not in Docs/scripting/api-reference.md");
        }

        [Test]
        public void EveryDocumentedNameExists()
        {
            var api = Api;
            var phantom = Reference.Select(r => r.Name).Distinct()
                .Where(name =>
                {
                    var colon = name.IndexOf(':');
                    if (colon < 0)
                        return !api.Globals.ContainsKey(name);
                    var method = name[(colon + 1)..];
                    return !ApiReference.TypesOf(name[..colon]).Any(t => api.Types.ContainsKey(t + ":" + method));
                })
                .ToList();

            Assert.That(phantom, Is.Empty, "documented in Docs/scripting/api-reference.md but not in the plugin's API");
        }

        [Test]
        public void EveryDocumentedNameHasADescription()
        {
            var bare = Reference.Where(r => r.Description.Length == 0).Select(r => r.Name).ToList();
            Assert.That(bare, Is.Empty);
        }

        [Test]
        public void EditorDefinitionsAreUpToDate()
        {
            var generated = DefinitionWriter.Write(Api, Reference);
            var path = Docs("tw.def.lua");

            if (Environment.GetEnvironmentVariable("TWLLINK_UPDATE_DEFS") != null)
            {
                File.WriteAllText(path, generated);
                Assert.Pass("Docs/scripting/tw.def.lua rewritten");
            }

            Assert.That(File.Exists(path), Is.True, "Docs/scripting/tw.def.lua is missing - run the tests with TWLLINK_UPDATE_DEFS=1");
            Assert.That(File.ReadAllText(path).Replace("\r\n", "\n"), Is.EqualTo(generated),
                "Docs/scripting/tw.def.lua is stale - run the tests with TWLLINK_UPDATE_DEFS=1 and commit it");
        }

        [Test]
        public void DefinitionsParseAsLua()
        {
            var text = DefinitionWriter.Write(Api, Reference);
            var tree = Loretta.CodeAnalysis.Lua.LuaSyntaxTree.ParseText(text, Linking.ModuleGraph.ParseOptions);
            Assert.That(tree.GetDiagnostics().Where(d => d.Severity == Loretta.CodeAnalysis.DiagnosticSeverity.Error), Is.Empty);
        }

        [Test]
        public void TheManifestSchemaHasExactlyTheFieldsTheLinkerReads()
        {
            using var schema = System.Text.Json.JsonDocument.Parse(File.ReadAllText(Docs("twproject.schema.json")));
            var properties = schema.RootElement.GetProperty("properties");
            string[] Names(System.Text.Json.JsonElement e) => e.EnumerateObject().Select(p => p.Name).ToArray();

            Assert.That(Names(properties), Is.EquivalentTo(Project.ManifestReader.TopLevelFields));
            Assert.That(Names(properties.GetProperty("build").GetProperty("properties")), Is.EquivalentTo(Project.ManifestReader.BuildFields));
            Assert.That(Names(properties.GetProperty("diagnostics").GetProperty("properties")), Is.EquivalentTo(Project.ManifestReader.DiagnosticsFields));
            Assert.That(schema.RootElement.GetProperty("required").EnumerateArray().Select(e => e.GetString()),
                Is.EquivalentTo(new[] { "format", "id", "name", "author", "version", "description", "api" }));
        }

        [Test]
        public void EveryDiagnosticCodeIsInTheAuthorsTableWithItsSeverity()
        {
            var rows = File.ReadAllLines(Docs("authoring.md"))
                .Where(l => l.StartsWith("| TW", StringComparison.Ordinal))
                .Select(l => l.Split('|', StringSplitOptions.TrimEntries))
                .ToDictionary(cells => cells[1], cells => cells[2]);

            Assert.That(rows.Keys, Is.EquivalentTo(Diagnostics.DiagnosticCodes.All), "codes in Docs/scripting/authoring.md vs DiagnosticCodes");
            foreach (var (code, severity) in rows)
            {
                var expected = Diagnostics.DiagnosticCodes.SeverityOf(code) == Diagnostics.Severity.Error ? "error" : "warning";
                Assert.That(severity, Is.EqualTo(expected), code);
            }
        }

        private static bool IsFieldOfDocumentedTable(string key, System.Collections.Generic.HashSet<string> documented)
        {
            var dot = key.LastIndexOf('.');
            return dot > 0 && documented.Contains(key[..dot]);
        }
    }
}
