using System.Collections.Generic;
using System.Linq;
using System.Text;

using Loretta.CodeAnalysis;
using Loretta.CodeAnalysis.Lua;
using Loretta.CodeAnalysis.Text;

using TweakerLuaLinker.Project;

namespace TweakerLuaLinker.Linking
{
    /// <summary>A line of output together with the source line it came from (1-based).</summary>
    internal readonly record struct SourceLine(string Text, int Line);

    /// <summary>
    /// A module's text as the lines that go into the bundle, each carrying its original line number - the
    /// raw material the line map is written from (lua-engine-fix-roadmap.md §9.6). Nothing here decides
    /// map entries; it only never loses track of where a line came from.
    /// </summary>
    internal static class SourceLines
    {
        public static List<SourceLine> Of(Module module, CommentMode mode)
        {
            var text = module.Text;
            var comments = mode == CommentMode.Strip ? CommentSpans(module) : [];

            var result = new List<SourceLine>(text.Lines.Count);
            var next = 0;
            foreach (var line in text.Lines)
            {
                var number = line.LineNumber + 1;
                var content = text.ToString(line.Span);

                if (mode == CommentMode.Keep)
                {
                    result.Add(new SourceLine(content, number));
                    continue;
                }

                // Comments are sorted and do not overlap; skip the ones that end before this line.
                while (next < comments.Count && comments[next].End <= line.Start)
                    next++;

                var stripped = Strip(content, line.Span, comments, next, out var touched, out var endsInComment);
                if (!touched)
                {
                    result.Add(new SourceLine(content, number));
                }
                else if (stripped.Trim().Length > 0)
                {
                    result.Add(new SourceLine(endsInComment ? stripped.TrimEnd() : stripped, number));
                }
                // else: a line that held nothing but comment goes, and its line with it (§9.4, build.comments).
            }

            // A file ending in a line break has an empty last "line" in SourceText; the bundle closes the
            // module on a line of its own anyway, so it adds nothing but a blank line before `end`.
            if (result.Count > 0 && result[^1].Text.Length == 0 && text.Length > 0 && text[text.Length - 1] is '\n' or '\r')
                result.RemoveAt(result.Count - 1);

            return result;
        }

        /// <summary>Every comment in the module, in order. Shebang lines count: they are not Lua inside a function.</summary>
        private static List<TextSpan> CommentSpans(Module module)
        {
            return module.Tree.GetRoot()
                .DescendantTrivia(descendIntoTrivia: true)
                .Where(t => t.Kind() is SyntaxKind.SingleLineCommentTrivia or SyntaxKind.MultiLineCommentTrivia or SyntaxKind.ShebangTrivia)
                .Select(t => t.Span)
                .OrderBy(s => s.Start)
                .ToList();
        }

        /// <summary>
        /// The line with every comment piece that falls on it cut out. A multi-line comment keeps its line
        /// breaks - each of its lines is cut separately - so code before and after it never ends up on one
        /// line. Where a cut would glue two tokens together (`a--[[x]]b`), a space stands in for it.
        /// </summary>
        private static string Strip(string content, TextSpan lineSpan, List<TextSpan> comments, int first, out bool touched, out bool endsInComment)
        {
            touched = false;
            endsInComment = false;

            var builder = new StringBuilder(content.Length);
            var cursor = lineSpan.Start;
            for (var i = first; i < comments.Count && comments[i].Start < lineSpan.End; i++)
            {
                var start = System.Math.Max(comments[i].Start, lineSpan.Start);
                var end = System.Math.Min(comments[i].End, lineSpan.End);
                if (end <= start)
                    continue;

                touched = true;
                builder.Append(content, cursor - lineSpan.Start, start - cursor);
                cursor = end;

                if (end == lineSpan.End)
                {
                    endsInComment = true;
                }
                else if (builder.Length > 0 && !char.IsWhiteSpace(builder[^1]) && !char.IsWhiteSpace(content[end - lineSpan.Start]))
                {
                    builder.Append(' ');
                }
            }

            if (!touched)
                return content;

            builder.Append(content, cursor - lineSpan.Start, lineSpan.End - cursor);
            return builder.ToString();
        }
    }
}
