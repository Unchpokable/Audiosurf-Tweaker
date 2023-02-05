using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace ASCommander.DllInject.Exceptions
{
    internal class MemoryAllocationException : Exception
    {
        public MemoryAllocationException()
        {
        }

        public MemoryAllocationException(string message) : base(message)
        {
        }

        public MemoryAllocationException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected MemoryAllocationException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}
