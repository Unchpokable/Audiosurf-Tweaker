using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace SkinChangerRestyle.Core.NetworkTools.Exceptions
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

        protected UnresolvedHostnameException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}
