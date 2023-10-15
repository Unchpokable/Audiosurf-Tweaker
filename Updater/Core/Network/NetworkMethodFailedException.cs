using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace Updater.Core.Network
{
    public class NetworkMethodFailedException : Exception
    {
        public NetworkMethodFailedException() : base() { }

        public NetworkMethodFailedException(string message) : base(message) { }

        public NetworkMethodFailedException(string message, Exception innerException) : base(message, innerException) { }

        public NetworkMethodFailedException(SerializationInfo info, StreamingContext context) : base(info, context) { } 
    }
}
