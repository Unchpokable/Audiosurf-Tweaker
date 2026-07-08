using System;

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
    }
}
