using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace TweakerScripts.Exceptions
{
    public class UnresolvedSymbolException : Exception
    {
        public UnresolvedSymbolException()
        {
        }

        public UnresolvedSymbolException(string message) : base(message)
        {
        }

        public UnresolvedSymbolException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected UnresolvedSymbolException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}
