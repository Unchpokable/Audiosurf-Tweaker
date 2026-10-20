using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

using NUnit.Framework;

namespace TweakerLuaLinker.Tests
{
    /// <summary>
    /// Tools/AgentKit - what Script Dev puts into a project for AI agents (dev-bundle-roadmap.md §3.5) - holds
    /// together: kit.json and the skill folders agree, every skill has the front matter all three agents need, the
    /// templates carry the placeholders AgentSetup fills, and every `.tweaker/docs/...` the kit points an agent at
    /// exists in Docs/ under that name. What the words say is DocsTests' (names, commands, links) and, in the end,
    /// an agent's (Ф6).
    /// </summary>
    [TestFixture]
    public class AgentKitTests
    {
        private static string Kit => Path.Combine(DocFiles.Root, "Tools", "AgentKit");

        private static string[] SkillFolders() =>
            Directory.EnumerateDirectories(Path.Combine(Kit, "skills")).Select(Path.GetFileName).OrderBy(n => n, StringComparer.Ordinal).ToArray()!;

        [Test]
        public void KitJsonListsExactlyTheSkillFolders()
        {
            using var json = JsonDocument.Parse(File.ReadAllText(Path.Combine(Kit, "kit.json")));
            var listed = json.RootElement.GetProperty("skills").EnumerateArray().Select(s => s.GetProperty("name").GetString()!).ToArray();

            Assert.Multiple(() =>
            {
                Assert.That(json.RootElement.GetProperty("format").GetInt32(), Is.EqualTo(1));
                Assert.That(json.RootElement.GetProperty("version").GetInt32(), Is.GreaterThan(0));
                Assert.That(listed, Is.Unique);
                Assert.That(listed, Is.EquivalentTo(SkillFolders()), "kit.json and skills/ disagree");
            });
        }

        [TestCaseSource(nameof(Skills))]
        public void SkillHasFrontMatterEveryAgentAccepts(string skill)
        {
            var text = File.ReadAllText(Path.Combine(Kit, "skills", skill, "SKILL.md")).Replace("\r\n", "\n");
            var front = Regex.Match(text, @"\A---\n(.*?)\n---\n", RegexOptions.Singleline);
            Assert.That(front.Success, Is.True, "SKILL.md must open with --- front matter ---");

            var fields = front.Groups[1].Value.Split('\n')
                .Select(line => line.Split(':', 2))
                .Where(kv => kv.Length == 2)
                .ToDictionary(kv => kv[0].Trim(), kv => kv[1].Trim(), StringComparer.Ordinal);

            Assert.Multiple(() =>
            {
                // Cursor: name is required, equals the folder, [a-z0-9-]. Codex: name and description required.
                Assert.That(fields.GetValueOrDefault("name"), Is.EqualTo(skill));
                Assert.That(skill, Does.Match("^[a-z0-9-]+$"));
                Assert.That(fields.GetValueOrDefault("description"), Is.Not.Null.And.Length.GreaterThan(80),
                    "the description is all an agent sees when deciding to open the skill - say what and when");
                Assert.That(fields.GetValueOrDefault("description")?.Length ?? 0, Is.LessThanOrEqualTo(1024));
                Assert.That(front.Groups[1].Value.Split('\n'), Has.Length.EqualTo(fields.Count), "one field per line, no multi-line values");

                var body = text[front.Length..].Split('\n').Length;
                Assert.That(body, Is.LessThan(500), "keep SKILL.md short; detail goes to references/");
            });
        }

        [Test]
        public void TemplatesCarryTheirPlaceholders()
        {
            var agents = File.ReadAllText(Path.Combine(Kit, "agents", "AGENTS.md"));
            var claude = File.ReadAllText(Path.Combine(Kit, "agents", "CLAUDE.md"));
            var stub = File.ReadAllText(Path.Combine(Kit, "agents", "claude-skill.md"));
            var block = File.ReadAllText(Path.Combine(Kit, "agents", "agents-block.md"));

            Assert.Multiple(() =>
            {
                Assert.That(agents, Does.Contain("{{project-name}}"));
                Assert.That(agents, Does.Contain("<!-- tweaker:begin -->\n{{block}}\n<!-- tweaker:end -->").Or.Contain("<!-- tweaker:begin -->\r\n{{block}}\r\n<!-- tweaker:end -->"));
                Assert.That(claude, Does.StartWith("@AGENTS.md"));
                Assert.That(stub, Does.Contain("name: {{name}}").And.Contain("description: {{description}}").And.Contain(".agents/skills/{{name}}/SKILL.md"));
                Assert.That(block, Does.Not.Contain("tweaker:begin").And.Not.Contain("tweaker:end"), "the block must not carry the markers that delimit it");

                // Codex reads at most 32 KiB of AGENTS.md along the whole path; the block is a small part of it.
                Assert.That(Encoding.UTF8.GetByteCount(block), Is.LessThan(8 * 1024));
            });
        }

        [Test]
        public void TheBlockNamesEveryShippedSkill()
        {
            var block = File.ReadAllText(Path.Combine(Kit, "agents", "agents-block.md"));
            var missing = SkillFolders().Where(skill => !block.Contains($"`{skill}`")).ToList();

            Assert.That(missing, Is.Empty, "AGENTS.md's table is how an agent learns which skill is for what");
        }

        [Test]
        public void DocsPathsTheKitGivesExist()
        {
            var docs = Path.Combine(DocFiles.Root, "Docs");
            var broken = new List<string>();
            var seen = 0;

            foreach (var file in DocFiles.Kit())
            {
                foreach (var code in Markdown.Code(File.ReadAllText(file)))
                {
                    foreach (Match m in Regex.Matches(code, @"\.tweaker/docs/([\w./-]*)(?:#([\w-]+))?"))
                    {
                        ++seen;
                        var relative = m.Groups[1].Value.TrimEnd('/');
                        var path = Path.Combine(docs, relative.Replace('/', Path.DirectorySeparatorChar));

                        if (relative.Split('/').FirstOrDefault() is "Internal" or "readme")
                            broken.Add($"{DocFiles.Relative(file)}: {m.Value} - does not ship");
                        else if (!File.Exists(path) && !Directory.Exists(path))
                            broken.Add($"{DocFiles.Relative(file)}: {m.Value}");
                        else if (m.Groups[2].Success && !Markdown.Anchors(File.ReadAllText(path)).Contains(m.Groups[2].Value))
                            broken.Add($"{DocFiles.Relative(file)}: {m.Value} - no such heading");
                    }
                }
            }

            Assert.That(seen, Is.GreaterThan(10), "the pattern stopped finding paths - check the regex, not the kit");
            Assert.That(broken, Is.Empty);
        }

        [Test]
        public void NoAbsolutePaths()
        {
            var found = DocFiles.Kit()
                .SelectMany(f => Regex.Matches(File.ReadAllText(f), @"(?<![\w\\])[A-Za-z]:\\\\?[A-Za-z]").Select(m => DocFiles.Relative(f) + ": " + m.Value))
                .Where(hit => !hit.StartsWith("Tools/AgentKit/README.md", StringComparison.Ordinal)) // the local.json example
                .ToList();

            Assert.That(found, Is.Empty, "a project's committed files hold no paths of anybody's computer (§2.17)");
        }

        private static IEnumerable<string> Skills() =>
            LuaRunner.RepositoryRoot() is { } root && Directory.Exists(Path.Combine(root, "Tools", "AgentKit", "skills"))
                ? Directory.EnumerateDirectories(Path.Combine(root, "Tools", "AgentKit", "skills")).Select(d => Path.GetFileName(d)!).OrderBy(n => n, StringComparer.Ordinal)
                : Enumerable.Empty<string>();
    }
}
