using System;

namespace TweakerUI.Core.NetworkTools.Exceptions
{
    internal class UnresolvedHostnameException : Exception
    {
        public UnresolvedHostnameException()
        {
        }

        public UnresolvedHostnameException(string message) : base(message)
        {
        }

        public UnresolvedHostnameException(string message, Exception innerException) : base(message, innerException)
        {
        }
    }
}