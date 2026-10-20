using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace TweakerLuaLinker.Api
{
    /// <summary>
    /// Docs/scripting/tw.def.lua: the tw API as Lua Language Server definitions (lua-engine-fix-roadmap.md
    /// §9.8) - what makes an editor complete tw.hud.te into tw.hud.text, show its parameters and its
    /// description, and underline a name that does not exist while it is being typed.
    ///
    /// Names and parameters come from api.json (the plugin's prelude), descriptions from
    /// api-reference.md. Neither is edited to suit this file, and this file is never edited by hand:
    /// TweakerLuaLinker.Tests regenerates it and fails when the committed copy is stale. twllink --new puts
    /// a copy into every new project.
    ///
    /// No parameter types: the reference does not state them in a form worth parsing, and a wrong type
    /// in a definition file is worse than none - the editor would flag correct code.
    /// </summary>
    internal static class DefinitionWriter
    {
        private const int Width = 100;

        public static string Write(ApiDescription api, IReadOnlyList<ReferenceEntry> reference)
        {
            var docs = reference.GroupBy(r => r.Name).ToDictionary(g => g.Key, g => g.ToList(), StringComparer.Ordinal);
            var w = new StringBuilder();

            w.Append("---@meta\n");
            w.Append("-- The Audiosurf Tweaker scripting API (tw.*), for Lua Language Server. API ").Append(api.Api).Append(".\n");
            w.Append("--\n");
            w.Append("-- Generated from Docs/scripting/api.json and Docs/scripting/api-reference.md; do not edit.\n");
            w.Append("-- In a project made by `twllink --new` this is already wired up (.luarc.json). Elsewhere, add the\n");
            w.Append("-- folder holding this file to Lua.workspace.library.\n");

            // Handle types first: the constructors below name them in their @return.
            foreach (var type in api.Types.Keys.Select(k => k[..k.IndexOf(':')]).Distinct().OrderBy(t => t, StringComparer.Ordinal))
            {
                w.Append('\n');
                w.Append("---@class tw.").Append(type).Append('\n');
                w.Append("local ").Append(type).Append(" = {}\n");

                foreach (var (key, entry) in api.Types.Where(t => t.Key.StartsWith(type + ":", StringComparison.Ordinal)).OrderBy(t => t.Key, StringComparer.Ordinal))
                {
                    var method = key[(type.Length + 1)..];
                    var receiver = ApiReference.ReceiversOf(type).FirstOrDefault(r => docs.ContainsKey(r + ":" + method));
                    var entries = ForType(type, receiver != null ? Docs(docs, receiver + ":" + method) : []);
                    w.Append('\n');
                    Describe(w, entries);
                    if (entry.Returns != null)
                        w.Append("---@return tw.").Append(entry.Returns).Append('\n');
                    w.Append("function ").Append(type).Append(':').Append(method).Append('(').Append(Params(entry)).Append(") end\n");
                }
            }

            // Then tw itself, parents before children so every table exists before a field is added to it.
            foreach (var (key, entry) in api.Globals.Where(g => g.Key == "tw" || g.Key.StartsWith("tw.", StringComparison.Ordinal))
                         .OrderBy(g => g.Key.Count(c => c == '.') > 0 ? g.Key[..g.Key.LastIndexOf('.')] : "", StringComparer.Ordinal)
                         .ThenBy(g => g.Key, StringComparer.Ordinal))
            {
                if (IsDataField(api, key))
                    continue;

                w.Append('\n');
                var entries = Docs(docs, key);
                if (entry.IsTable && api.Globals.Any(g => g.Key.StartsWith(key + ".", StringComparison.Ordinal) && g.Value.IsFunction))
                {
                    Describe(w, entries);
                    w.Append("---@class ").Append(key).Append('\n');
                    w.Append(key).Append(" = {}\n");
                }
                else if (entry.IsTable)
                {
                    // A table of values (tw.hud.corners): written out, so its fields complete too.
                    Describe(w, entries);
                    var fields = api.Fields(key).OrderBy(f => f, StringComparer.Ordinal).Select(f => $"{f} = 0");
                    w.Append(key).Append(" = { ").Append(string.Join(", ", fields)).Append(" }\n");
                }
                else if (entry.IsFunction)
                {
                    Describe(w, entries);
                    if (entry.Returns != null)
                        w.Append("---@return tw.").Append(entry.Returns).Append('\n');
                    w.Append("function ").Append(key).Append('(').Append(Params(entry)).Append(") end\n");
                }
                else
                {
                    Describe(w, entries);
                    w.Append("---@type ").Append(entry.Kind).Append('\n');
                    w.Append(key).Append(" = ").Append(entry.Kind == "string" ? "\"\"" : "0").Append('\n');
                }
            }

            return w.ToString();
        }

        /// <summary>A field of a table that is itself a value (tw.hud.corners.top_left) - written with its table.</summary>
        private static bool IsDataField(ApiDescription api, string key)
        {
            var dot = key.LastIndexOf('.');
            if (dot < 0)
                return false;
            var parent = key[..dot];
            return api.Globals.TryGetValue(parent, out var p) && p.IsTable
                   && !api.Globals.Any(g => g.Key.StartsWith(parent + ".", StringComparison.Ordinal) && g.Value.IsFunction);
        }

        private static List<ReferenceEntry> Docs(Dictionary<string, List<ReferenceEntry>> docs, string name)
        {
            return docs.TryGetValue(name, out var list) ? list : [];
        }

        /// <summary>
        /// The forms of a method that belong to one kind of handle: `handle:set(value)` is marked
        /// *(float handles)*, so FloatHandle:set gets that one and not the vector and matrix forms. Forms
        /// without a note belong to every kind.
        /// </summary>
        private static List<ReferenceEntry> ForType(string type, List<ReferenceEntry> entries)
        {
            var mine = entries.Where(e => e.Note == null || NoteMatches(e.Note, type)).ToList();
            return mine.Count > 0 ? mine : entries;
        }

        private static bool NoteMatches(string note, string type)
        {
            return type switch
            {
                "FloatHandle" => note.Contains("float", StringComparison.Ordinal),
                "TextHandle" => note.Contains("text", StringComparison.Ordinal),
                "VectorHandle" => note.Contains("vector", StringComparison.Ordinal),
                "MatrixHandle" => note.Contains("matrix", StringComparison.Ordinal),
                "ArrayHandle" => note == "tw.array",
                "VectorArrayHandle" => note == "tw.array_vec",
                _ => true,
            };
        }

        private static string Params(ApiEntry entry)
        {
            var names = (entry.Params ?? []).ToList();
            if (entry.Vararg)
                names.Add("...");
            return string.Join(", ", names);
        }

        /// <summary>
        /// The reference's description, and - when one name has several documented forms, like
        /// handle:set - each form with what it returns, so the hover shows all of them.
        /// </summary>
        private static void Describe(StringBuilder w, List<ReferenceEntry> entries)
        {
            if (entries.Count == 0)
                return;

            var descriptions = entries.Select(e => e.Description).Where(d => d.Length > 0).Distinct().ToList();
            foreach (var paragraph in descriptions)
            {
                // A list item per line, as the reference has it; everything else wrapped.
                foreach (var part in paragraph.Split('\n'))
                {
                    foreach (var line in Wrap(part))
                        w.Append("---").Append(line).Append('\n');
                }
                w.Append("---\n");
            }

            foreach (var e in entries)
            {
                w.Append("---`").Append(e.Signature).Append('`');
                if (e.Returns != null)
                    w.Append(" → ").Append(e.Returns);
                w.Append('\n');
            }

            if (entries.Any(e => e.Deprecated))
                w.Append("---@deprecated\n");
        }

        private static IEnumerable<string> Wrap(string text)
        {
            var line = new StringBuilder();
            foreach (var word in text.Split(' ', StringSplitOptions.RemoveEmptyEntries))
            {
                if (line.Length > 0 && line.Length + 1 + word.Length > Width)
                {
                    yield return line.ToString();
                    line.Clear();
                }
                if (line.Length > 0)
                    line.Append(' ');
                line.Append(word);
            }
            if (line.Length > 0)
                yield return line.ToString();
        }
    }
}
