using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace ASCommander.DllInject
{
    internal class ProcessNotFoundException : Exception
    {
        public ProcessNotFoundException()
        {
        }

        public ProcessNotFoundException(string message) : base(message)
        {
        }

        public ProcessNotFoundException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected ProcessNotFoundException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}
