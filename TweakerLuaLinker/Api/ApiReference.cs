using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;

namespace TweakerLuaLinker.Api
{
    /// <summary>One name as Docs/scripting/api-reference.md documents it.</summary>
    /// <remarks>
    /// <see cref="Note"/> is the italic note after a heading - "float handles", "`tw.array`" - which is how
    /// the reference says that one form of a method belongs to one kind of handle.
    /// </remarks>
    internal sealed record ReferenceEntry(string Name, string Signature, string? Returns, string Description, bool Deprecated, string? Note);

    /// <summary>
    /// The names Docs/scripting/api-reference.md documents, read off its `### ` headings: one heading per
    /// signature, several headings stacked over one shared description, several signatures in one heading
    /// separated by " / ". Only what the reference already looks like - it is written for people, and
    /// this reads it rather than making it any less readable.
    ///
    /// Methods are documented against the value a script holds, not against a type: `handle:get()`
    /// covers every channel handle, `array:` both array kinds, `mute:` the mute handle; for script tabs
    /// (API 3) `tab:` is the tab, `parent:` a tab or a group - what controls are declared on - and `control:`
    /// any node on it, with `choice:`, `list:`, `button:` and `paragraph:` for what only one kind has.
    /// <see cref="TypesOf"/> is that mapping, and the one place it is written down. Receivers overlap; where
    /// two document one method, the narrower one (<see cref="ReceiversOf"/>, narrowest first) describes it.
    /// </summary>
    internal static class ApiReference
    {
        private static readonly Regex _code = new(@"`([^`]+)`", RegexOptions.CultureInvariant);
        /// <summary>Every receiver the reference documents methods on.</summary>
        public static readonly IReadOnlyList<string> Receivers =
            ["handle", "array", "mute", "tab", "parent", "control", "choice", "list", "button", "paragraph"];

        private static readonly Regex _name = new(
            @"^(tw(?:\.[A-Za-z_][A-Za-z0-9_]*)+|(?:" + string.Join("|", Receivers) + @"):[A-Za-z_][A-Za-z0-9_]*)", RegexOptions.CultureInvariant);

        public static IReadOnlyList<string> TypesOf(string receiver)
        {
            return receiver switch
            {
                "handle" => ["FloatHandle", "TextHandle", "VectorHandle", "MatrixHandle"],
                "array" => ["ArrayHandle", "VectorArrayHandle"],
                "mute" => ["MuteHandle"],
                "tab" => ["TabHandle"],
                "parent" => ["TabHandle", "GroupHandle"],
                "control" => ["ControlHandle", "ChoiceHandle", "ListHandle", "ButtonHandle", "ParagraphHandle", "GroupHandle", "SeparatorHandle"],
                "choice" => ["ChoiceHandle"],
                "list" => ["ListHandle"],
                "button" => ["ButtonHandle"],
                "paragraph" => ["ParagraphHandle"],
                _ => [],
            };
        }

        /// <summary>The receivers whose methods a handle type has, narrowest first.</summary>
        public static IReadOnlyList<string> ReceiversOf(string type)
        {
            return Receivers.Where(r => TypesOf(r).Contains(type)).OrderBy(r => TypesOf(r).Count).ToList();
        }

        public static IReadOnlyList<ReferenceEntry> Parse(string markdown)
        {
            var lines = markdown.Replace("\r\n", "\n").Split('\n');
            var result = new List<ReferenceEntry>();
            var pending = new List<(string Name, string Signature, string? Returns, bool Deprecated, string? Note)>();
            var inCode = false;

            for (var i = 0; i < lines.Length; i++)
            {
                var line = lines[i];
                if (line.StartsWith("```", StringComparison.Ordinal))
                {
                    inCode = !inCode;
                    continue;
                }
                if (inCode)
                    continue;

                if (line.StartsWith("### ", StringComparison.Ordinal))
                {
                    var returns = ReturnsOf(line);
                    var deprecated = line.Contains("*(deprecated)*", StringComparison.Ordinal);
                    var note = NoteOf(line);
                    foreach (Match code in _code.Matches(line))
                    {
                        var signature = code.Groups[1].Value;
                        var name = _name.Match(signature);
                        if (name.Success)
                            pending.Add((name.Value, signature, returns, deprecated, note));
                    }
                    continue;
                }

                if (pending.Count > 0 && line.Trim().Length > 0)
                {
                    // The first paragraph after a group of headings is the description of all of them. A
                    // list stays a list: each "- " item starts a line of its own.
                    var description = new System.Text.StringBuilder();
                    for (; i < lines.Length && lines[i].Trim().Length > 0 && !lines[i].StartsWith("#", StringComparison.Ordinal); i++)
                    {
                        var text = lines[i].Trim();
                        if (description.Length > 0)
                            description.Append(text.StartsWith("- ", StringComparison.Ordinal) ? '\n' : ' ');
                        description.Append(text);
                    }
                    i--;

                    result.AddRange(pending.Select(p => new ReferenceEntry(p.Name, p.Signature, p.Returns, description.ToString(), p.Deprecated, p.Note)));
                    pending.Clear();
                }
            }

            result.AddRange(pending.Select(p => new ReferenceEntry(p.Name, p.Signature, p.Returns, "", p.Deprecated, p.Note)));
            return result;
        }

        /// <summary>"float handles" off "### `handle:set(value)` → boolean *(float handles)*".</summary>
        private static string? NoteOf(string heading)
        {
            var start = heading.LastIndexOf(" *(", StringComparison.Ordinal);
            var end = heading.LastIndexOf(")*", StringComparison.Ordinal);
            if (start < 0 || end < start)
                return null;
            var note = heading[(start + 3)..end].Replace("`", "");
            return note == "deprecated" ? null : note;
        }

        /// <summary>"→ width, height" off a heading, without the italic notes after it.</summary>
        private static string? ReturnsOf(string heading)
        {
            var arrow = heading.IndexOf('→');
            if (arrow < 0)
                return null;
            var text = heading[(arrow + 1)..];
            var note = text.IndexOf(" *(", StringComparison.Ordinal);
            if (note >= 0)
                text = text[..note];
            return text.Replace("`", "").Replace("*", "").Trim();
        }
    }
}
