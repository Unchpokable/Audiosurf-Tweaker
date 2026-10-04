using TweakerCore.Scripting;

namespace TweakerUI.Models
{
    /// <summary>One line of a build's diagnostics in the Script Dev tab - twllink's --json entry, as text to show.</summary>
    public sealed class DiagnosticRowViewModel
    {
        public DiagnosticRowViewModel(TwllinkDiagnostic diagnostic)
        {
            Diagnostic = diagnostic;
        }

        public TwllinkDiagnostic Diagnostic { get; }

        public bool IsError => Diagnostic.Severity == TwllinkSeverity.Error;

        public string Code => Diagnostic.Code;

        public string Message => Diagnostic.Message;

        /// <summary>"src/main.lua:3:11" - or "project" for a message about the project as a whole.</summary>
        public string Location
        {
            get
            {
                if (string.IsNullOrEmpty(Diagnostic.File))
                    return "project";
                if (Diagnostic.Line <= 0)
                    return Diagnostic.File;
                return Diagnostic.Column > 0
                    ? Diagnostic.File + ":" + Diagnostic.Line + ":" + Diagnostic.Column
                    : Diagnostic.File + ":" + Diagnostic.Line;
            }
        }
    }
}
