using System.Collections.Generic;
using System.Linq;

namespace TweakerLuaLinker.Diagnostics
{
    internal enum Severity
    {
        Warning,
        Error,
    }

    /// <summary>
    /// One thing the linker has to say about a project. The same record feeds both outputs - the human line
    /// and the --json entry (lua-engine-fix-roadmap.md §9.7) - so they cannot disagree.
    ///
    /// <see cref="File"/> is relative to the project root with forward slashes, or empty when the message is
    /// about the project as a whole. <see cref="Line"/> and <see cref="Column"/> are 1-based, 0 when unknown.
    /// </summary>
    internal sealed record Diagnostic(Severity Severity, string Code, string File, int Line, int Column, string Message);

    /// <summary>
    /// Collects diagnostics while a build runs and applies the project's diagnostics settings at the end,
    /// so no stage has to know about suppression or warnings-as-errors.
    /// </summary>
    internal sealed class DiagnosticBag
    {
        private readonly List<Diagnostic> _items = new();

        public IReadOnlyList<Diagnostic> Items => _items;

        public bool HasErrors => _items.Any(d => d.Severity == Severity.Error);

        public void Add(string code, string file, int line, int column, string message)
        {
            _items.Add(new Diagnostic(DiagnosticCodes.SeverityOf(code), code, file, line, column, message));
        }

        public void Add(string code, string file, string message)
        {
            Add(code, file, 0, 0, message);
        }

        /// <summary>
        /// Drops suppressed warnings and promotes the rest when asked to. Errors are never suppressed: a
        /// project that does not build cannot be made to build by a setting.
        /// </summary>
        public void ApplySettings(IReadOnlyCollection<string> suppress, bool warningsAsErrors)
        {
            for (var i = _items.Count - 1; i >= 0; i--)
            {
                var item = _items[i];
                if (item.Severity != Severity.Warning)
                    continue;

                if (suppress.Contains(item.Code))
                    _items.RemoveAt(i);
                else if (warningsAsErrors)
                    _items[i] = item with { Severity = Severity.Error };
            }
        }

        /// <summary>Stable order for output: by file, then position, then code - independent of which stage found what.</summary>
        public IReadOnlyList<Diagnostic> Sorted()
        {
            return _items
                .OrderBy(d => d.File, System.StringComparer.Ordinal)
                .ThenBy(d => d.Line)
                .ThenBy(d => d.Column)
                .ThenBy(d => d.Code, System.StringComparer.Ordinal)
                .ToList();
        }
    }
}
