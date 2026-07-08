using System;

namespace TweakerScripts.Exceptions
{
    public class ScriptParsingException : Exception
    {
        public ScriptParsingException()
        {
        }

        public ScriptParsingException(string message) : base(message)
        {
        }

        public ScriptParsingException(string message, Exception innerException) : base(message, innerException)
        {
        }
    }
}
