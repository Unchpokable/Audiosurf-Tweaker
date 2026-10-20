using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

using NUnit.Framework;

using TweakerLuaLinker.Api;

namespace TweakerLuaLinker.Tests
{
    /// <summary>
    /// What the public documentation and the agent kit say about things that exist elsewhere - files, headings,
    /// <c>tw.*</c> names, CgrPy commands - checked against those things (dev-bundle-roadmap.md §5). The public docs
    /// ship in DevTools\Docs and the kit in DevTools\AgentKit, away from the repository: a link that only works on
    /// GitHub's rendering of the repo, or a name nobody can call, is found here rather than by a reader.
    /// </summary>
    [TestFixture]
    public class DocsTests
    {
        [Test]
        public void RelativeLinksAndAnchorsResolve()
        {
            var broken = new List<string>();
            foreach (var file in DocFiles.Public().Concat(DocFiles.Kit()))
            {
                var text = Markdown.WithoutCode(File.ReadAllText(file));
                foreach (Match link in Regex.Matches(text, @"\]\(([^)\s]+)\)"))
                {
                    var target = link.Groups[1].Value;
                    if (Regex.IsMatch(target, "^[a-z][a-z0-9+.-]*:", RegexOptions.IgnoreCase))
                        continue;

                    var hash = target.IndexOf('#');
                    var path = hash < 0 ? target : target[..hash];
                    var anchor = hash < 0 ? null : target[(hash + 1)..];
                    var destination = path.Length == 0 ? file : Path.GetFullPath(Path.Combine(Path.GetDirectoryName(file)!, path));

                    if (!File.Exists(destination) && !Directory.Exists(destination))
                        broken.Add($"{DocFiles.Relative(file)}: no file {target}");
                    else if (anchor != null && destination.EndsWith(".md", StringComparison.OrdinalIgnoreCase)
                        && !Markdown.Anchors(File.ReadAllText(destination)).Contains(anchor))
                        broken.Add($"{DocFiles.Relative(file)}: no heading {target}");
                }
            }

            Assert.That(broken, Is.Empty);
        }

        [Test]
        public void EveryTwNameExists()
        {
            var api = ApiDescription.Load(Path.Combine(DocFiles.Root, "Docs", "scripting", "api.json"));
            var files = DocFiles.Kit().Concat(DocFiles.Under("Docs", "internals")).Append(Path.Combine(DocFiles.Root, "Docs", "internals.md"));

            var unknown = new List<string>();
            foreach (var file in files)
            {
                foreach (Match m in Regex.Matches(File.ReadAllText(file), @"(?<![\w.])tw\.[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*(\*)?"))
                {
                    var name = m.Value.TrimEnd('*');
                    var last = name[(name.LastIndexOf('.') + 1)..];
                    if (last is "lua" or "json")
                        continue; // a file name: tw.def.lua

                    var known = m.Groups[1].Success
                        ? api.Globals.Keys.Any(k => k.StartsWith(name, StringComparison.Ordinal))
                        : api.Globals.ContainsKey(name);
                    if (!known)
                        unknown.Add($"{DocFiles.Relative(file)}: {m.Value}");
                }
            }

            Assert.That(unknown.Distinct(), Is.Empty, "not in Docs/scripting/api.json");
        }

        [Test]
        public void EveryCgrpyCommandExists()
        {
            var tools = Path.Combine(DocFiles.Root, "Tools", "CgrPy", "rdy2use");
            var files = DocFiles.Public().Concat(DocFiles.Kit()).Append(Path.Combine(DocFiles.Root, "Tools", "CgrPy", "README.md"));

            var unknown = new List<string>();
            var seen = 0;
            foreach (var file in files)
            {
                foreach (var code in Markdown.Code(File.ReadAllText(file)))
                {
                    foreach (Match m in Regex.Matches(code, @"(?<![\w.\\/-])cgrpy(?:\.cmd)? ([a-z_]+)(?![\w\\/.])(?: ([a-z_]+)(?![\w\\/.]))?"))
                    {
                        ++seen;
                        var tool = m.Groups[1].Value;
                        var source = Path.Combine(tools, tool + ".py");
                        if (!File.Exists(source))
                        {
                            unknown.Add($"{DocFiles.Relative(file)}: {m.Value}");
                            continue;
                        }

                        // A tool with sub-commands dispatches through `COMMANDS = dict(name=function, ...)`; a word after
                        // it that is not one of those is a typo in the docs, not an argument.
                        var table = Regex.Match(File.ReadAllText(source), @"^COMMANDS\s*=\s*dict\(([^)]*)\)", RegexOptions.Multiline);
                        if (m.Groups[2].Success && table.Success)
                        {
                            var commands = Regex.Matches(table.Groups[1].Value, @"(\w+)\s*=").Select(c => c.Groups[1].Value);
                            if (!commands.Contains(m.Groups[2].Value))
                                unknown.Add($"{DocFiles.Relative(file)}: {m.Value}");
                        }
                    }
                }
            }

            Assert.That(seen, Is.GreaterThan(20), "the pattern stopped finding commands - check the regex, not the docs");
            Assert.That(unknown.Distinct(), Is.Empty, "no such tool in Tools/CgrPy/rdy2use, or no such sub-command");
        }

        [Test]
        public void SlugsFollowGitHub()
        {
            Assert.Multiple(() =>
            {
                Assert.That(Markdown.Slug("Mods and the leaderboards"), Is.EqualTo("mods-and-the-leaderboards"));
                Assert.That(Markdown.Slug("`tw.state()` → string"), Is.EqualTo("twstate--string"));
                Assert.That(Markdown.Slug("`twproject.json`"), Is.EqualTo("twprojectjson"));
                Assert.That(Markdown.Slug("Ф3 — итоги"), Is.EqualTo("ф3--итоги"));
            });

            var anchors = Markdown.Anchors("# A\n## A\n```\n# Not a heading\n```\n### A\n");
            Assert.That(anchors, Is.EquivalentTo(new[] { "a", "a-1", "a-2" }), "repeats get -1, -2; code is not headings");
        }
    }

    internal static class DocFiles
    {
        public static string Root => LuaRunner.RepositoryRoot() ?? throw new InconclusiveException("repository not found");

        /// <summary>Everything under Docs/ that ships: all of it but Internal/ and readme/ (CopyDevTools).</summary>
        public static IEnumerable<string> Public()
        {
            var docs = Path.Combine(Root, "Docs");
            return Directory.EnumerateFiles(docs, "*.md", SearchOption.AllDirectories)
                .Where(f => Path.GetRelativePath(docs, f).Split(Path.DirectorySeparatorChar)[0] is not ("Internal" or "readme"))
                .OrderBy(f => f, StringComparer.Ordinal);
        }

        public static IEnumerable<string> Kit() => Under("Tools", "AgentKit");

        public static IEnumerable<string> Under(params string[] parts)
        {
            var folder = Path.Combine(new[] { Root }.Concat(parts).ToArray());
            return Directory.Exists(folder)
                ? Directory.EnumerateFiles(folder, "*.md", SearchOption.AllDirectories).OrderBy(f => f, StringComparer.Ordinal)
                : Enumerable.Empty<string>();
        }

        public static string Relative(string file) => Path.GetRelativePath(Root, file).Replace('\\', '/');
    }

    internal static class Markdown
    {
        private static readonly Regex Fence = new(@"^```[^\n]*\n.*?^```", RegexOptions.Singleline | RegexOptions.Multiline);
        private static readonly Regex Inline = new(@"`[^`\n]+`");

        public static string WithoutFences(string text) => Fence.Replace(text.Replace("\r\n", "\n"), "");

        public static string WithoutCode(string text) => Inline.Replace(WithoutFences(text), "");

        /// <summary>Fenced blocks and inline spans - where commands and paths are written.</summary>
        public static IEnumerable<string> Code(string text)
        {
            text = text.Replace("\r\n", "\n");
            foreach (Match block in Fence.Matches(text))
                yield return block.Value;
            foreach (Match span in Inline.Matches(WithoutFences(text)))
                yield return span.Value.Trim('`');
        }

        /// <summary>
        /// GitHub's anchor for a heading: lower case, everything but letters, digits, '_', '-' and spaces dropped,
        /// spaces to '-'. Formatting characters go with the rest, which is why `x` and x give the same anchor.
        /// </summary>
        public static string Slug(string heading)
        {
            var s = Regex.Replace(heading.Trim().ToLowerInvariant(), @"[^\w\- ]", "");
            return s.Replace(' ', '-');
        }

        public static HashSet<string> Anchors(string text)
        {
            var anchors = new HashSet<string>(StringComparer.Ordinal);
            foreach (Match m in Regex.Matches(WithoutFences(text), @"^#{1,6}[ \t]+(.+?)[ \t]*$", RegexOptions.Multiline))
            {
                var slug = Slug(m.Groups[1].Value);
                var unique = slug;
                for (var n = 1; anchors.Contains(unique); ++n)
                    unique = $"{slug}-{n}";
                anchors.Add(unique);
            }
            return anchors;
        }
    }
}
