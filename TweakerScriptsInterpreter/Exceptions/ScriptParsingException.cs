using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace TweakerScripts.Exceptions
{
    internal class ScriptParsingException : Exception
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

        protected ScriptParsingException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}
