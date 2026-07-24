using System;

namespace AudiosurfInterface.Bridge
{
    public enum AsBridgeDiagnosticLevel
    {
        Warning,
        Error
    }

    /// <summary>
    /// A failure this module would otherwise have swallowed silently (bridge process spawn, pipe
    /// connect/read/write, reinitialize) - callers with a logging facility (AudiosurfInterface itself
    /// has none and takes no project references) can subscribe to AsBridgeConnection.Diagnostic /
    /// AudiosurfHandle.Diagnostic and record these however they like.
    /// </summary>
    public sealed class AsBridgeDiagnostic
    {
        public AsBridgeDiagnostic(AsBridgeDiagnosticLevel level, string context, string message, Exception exception = null)
        {
            Level = level;
            Context = context;
            Message = message;
            Exception = exception;
        }

        public AsBridgeDiagnosticLevel Level { get; }
        public string Context { get; }
        public string Message { get; }
        public Exception Exception { get; }
    }
}
