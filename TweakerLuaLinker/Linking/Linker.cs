using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

using Loretta.CodeAnalysis;
using Loretta.CodeAnalysis.Lua;

using TweakerLuaLinker.Api;
using TweakerLuaLinker.Diagnostics;
using TweakerLuaLinker.Project;

using Diagnostic = TweakerLuaLinker.Diagnostics.Diagnostic;

namespace TweakerLuaLinker.Linking
{
    internal sealed class LinkResult
    {
        public Manifest? Manifest { get; init; }

        /// <summary>The bundle text, or null when the build failed.</summary>
        public string? Bundle { get; init; }

        /// <summary>The .map text, null exactly when <see cref="Bundle"/> is.</summary>
        public string? Map { get; init; }

        public IReadOnlyList<Module> Modules { get; init; } = [];
        public IReadOnlyList<Diagnostic> Diagnostics { get; init; } = [];

        public bool Success => Bundle != null;
    }

    /// <summary>
    /// Project in, one script out (lua-engine-fix-roadmap.md §9.4-§9.6). Reads files, writes none - putting
    /// the result on disk is the caller's business, so the whole build can be checked in memory.
    ///
    /// Order of work, fixed by §9.6: parse, graph, transform (comments), write - and the line map is taken
    /// while writing, never computed ahead.
    /// </summary>
    internal static class Linker
    {
        /// <summary>Printed into every bundle's @bundle line. A change here changes every bundle's bytes.</summary>
        public const string ToolVersion = "1.1";

        // Bundle-level names every module can see as upvalues. Long and prefixed so no module's global of
        // the same name gets captured by accident; `require` alone is shadowed on purpose.
        private const string ModulesTable = "__twl_modules";
        private const string CacheTable = "__twl_cache";
        private const string LoadingMark = "__twl_loading";

        public static LinkResult Link(string projectRoot, ApiDescription? api = null)
        {
            api ??= ApiDescription.Embedded;
            var bag = new DiagnosticBag();
            var manifestPath = Path.Combine(projectRoot, Manifest.FileName);
            if (!File.Exists(manifestPath))
            {
                bag.Add(DiagnosticCodes.ManifestMissing, Manifest.FileName, $"no {Manifest.FileName} in {projectRoot}");
                return new LinkResult { Diagnostics = bag.Sorted() };
            }

            var manifest = ManifestReader.Read(manifestPath, bag);
            if (manifest == null)
                return new LinkResult { Diagnostics = bag.Sorted() };

            // A project written for a newer API than this twllink knows cannot be checked against it, and
            // would be built for a plugin that does not exist yet as far as this tool can tell.
            if (manifest.Api > api.Api)
            {
                bag.Add(DiagnosticCodes.ApiTooNew, Manifest.FileName,
                    $"the project targets API {manifest.Api}, and this twllink knows API {api.Api} - update the Tweaker, or lower \"api\"");
                return new LinkResult { Manifest = manifest, Diagnostics = bag.Sorted() };
            }

            var embeds = ReadEmbeds(projectRoot, manifest, bag);
            var graph = ModuleGraph.Build(projectRoot, bag);
            ApiChecker.Check(graph.Loaded, api, manifest.Api, bag);

            string? bundle = null;
            string? map = null;
            if (!bag.HasErrors)
            {
                var writer = Write(manifest, embeds, graph.Order);
                bundle = writer.Text();
                map = writer.MapText();
                Verify(manifest, bundle, bag);
            }

            bag.ApplySettings(manifest.Suppress, manifest.WarningsAsErrors);
            var success = !bag.HasErrors;
            return new LinkResult
            {
                Manifest = manifest,
                Bundle = success ? bundle : null,
                Map = success ? map : null,
                Modules = graph.Order,
                Diagnostics = bag.Sorted(),
            };
        }

        private static List<(string Path, string Text)> ReadEmbeds(string projectRoot, Manifest manifest, DiagnosticBag bag)
        {
            var result = new List<(string, string)>();
            var root = Path.GetFullPath(projectRoot);
            foreach (var entry in manifest.Embed)
            {
                // The manifest describes the project, not the machine (§9.4): an embedded file lives inside it.
                var full = Path.GetFullPath(Path.Combine(root, entry));
                var relative = Path.GetRelativePath(root, full);
                if (Path.IsPathRooted(entry) || relative.StartsWith("..", StringComparison.Ordinal) || Path.IsPathRooted(relative))
                {
                    bag.Add(DiagnosticCodes.EmbedOutsideProject, Manifest.FileName,
                        $"build.embed names '{entry}', which is outside the project - embedded files must live in the project folder");
                    continue;
                }
                if (!File.Exists(full))
                {
                    bag.Add(DiagnosticCodes.EmbedMissing, Manifest.FileName, $"build.embed names '{entry}', which does not exist");
                    continue;
                }

                string text;
                try
                {
                    var bytes = File.ReadAllBytes(full);
                    var start = bytes.Length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF ? 3 : 0;
                    text = new UTF8Encoding(false, true).GetString(bytes, start, bytes.Length - start);
                }
                catch (Exception e) when (e is IOException or UnauthorizedAccessException or DecoderFallbackException)
                {
                    bag.Add(DiagnosticCodes.EmbedNotText, Manifest.FileName, $"build.embed names '{entry}', which is not readable UTF-8 text: {e.Message}");
                    continue;
                }
                if (text.Contains('\0'))
                {
                    bag.Add(DiagnosticCodes.EmbedNotText, Manifest.FileName, $"build.embed names '{entry}', which is not a text file");
                    continue;
                }

                result.Add((relative.Replace('\\', '/'), text));
            }
            return result;
        }

        private static BundleWriter Write(Manifest manifest, List<(string Path, string Text)> embeds, IReadOnlyList<Module> modules)
        {
            var w = new BundleWriter();

            // The header the plugin reads (lua_script.cxx, read_header) - it stops at the first line that is
            // neither blank nor a comment, which is why the first line of code comes before anything embedded.
            w.Own($"-- @name        {manifest.Name}");
            w.Own($"-- @author      {manifest.Author}");
            w.Own($"-- @version     {manifest.Version}");
            w.Own($"-- @description {manifest.Description}");
            w.Own($"-- @api         {manifest.Api}");
            w.Own($"-- @bundle      twllink {ToolVersion}, {modules.Count} {(modules.Count == 1 ? "module" : "modules")}");
            w.Own("");
            w.Own($"local {ModulesTable}, {CacheTable}, {LoadingMark} = {{}}, {{}}, {{}}");

            foreach (var (path, text) in embeds)
            {
                var level = LongBracketLevel(text);
                w.Own("");
                w.Own($"--[{level}[ {path}");
                foreach (var line in SplitLines(text))
                    w.Own(line);
                w.Own($"]{level}]");
            }

            // A local require: the sandbox took the global one away and the bundle does not bring it back
            // (§9.5). Modules get it as a parameter, the way Lua hands a module its name as `...`.
            w.Own("");
            w.Own("local function require(name)");
            w.Own($"    local hit = {CacheTable}[name]");
            w.Own($"    if hit == {LoadingMark} then");
            w.Own("        error(\"module '\" .. tostring(name) .. \"' is still loading - a require cycle, or it failed while loading\", 2)");
            w.Own("    end");
            w.Own("    if hit ~= nil then return hit end");
            w.Own($"    local body = {ModulesTable}[name]");
            w.Own("    if body == nil then error(\"module '\" .. tostring(name) .. \"' is not in this bundle\", 2) end");
            w.Own($"    {CacheTable}[name] = {LoadingMark}");
            w.Own("    local value = body(require, name)");
            w.Own("    if value == nil then value = true end");
            w.Own($"    {CacheTable}[name] = value");
            w.Own("    return value");
            w.Own("end");

            // No indentation inside the wrapper: a line keeps its columns, so a column in an error message
            // needs no map at all.
            foreach (var module in modules)
            {
                w.Own("");
                w.Own($"{ModulesTable}[\"{module.Name}\"] = function(require, ...)");
                foreach (var line in SourceLines.Of(module, manifest.Comments))
                    w.Source(module.SourcePath, line.Line, line.Text);
                w.Own("end");
            }

            w.Own("");
            w.Own($"require(\"{ModuleGraph.MainModule}\")");
            return w;
        }

        /// <summary>The first long-bracket level whose closing bracket does not occur in the text: "", "=", "==", ...</summary>
        private static string LongBracketLevel(string text)
        {
            for (var n = 0; ; n++)
            {
                var level = new string('=', n);
                if (!text.Contains("]" + level + "]", StringComparison.Ordinal))
                    return level;
            }
        }

        private static IEnumerable<string> SplitLines(string text)
        {
            var lines = text.Replace("\r\n", "\n").Replace('\r', '\n').Split('\n');
            var count = lines.Length > 0 && lines[^1].Length == 0 ? lines.Length - 1 : lines.Length;
            return lines.Take(count);
        }

        /// <summary>
        /// Every module parsed on its own; the bundle is checked as a whole anyway. A failure here is a linker
        /// bug, not the author's - but shipping a script that does not load is worse than saying so.
        /// </summary>
        private static void Verify(Manifest manifest, string bundle, DiagnosticBag bag)
        {
            var tree = LuaSyntaxTree.ParseText(bundle, ModuleGraph.ParseOptions, manifest.OutputFileName);
            foreach (var error in tree.GetDiagnostics().Where(d => d.Severity == DiagnosticSeverity.Error).Take(5))
            {
                var span = error.Location.GetLineSpan().StartLinePosition;
                bag.Add(DiagnosticCodes.BundleInvalid, manifest.OutputFileName, span.Line + 1, span.Character + 1,
                    $"the generated bundle does not parse ({error.GetMessage()}) - this is a twllink bug, please report it");
            }
        }
    }
}
