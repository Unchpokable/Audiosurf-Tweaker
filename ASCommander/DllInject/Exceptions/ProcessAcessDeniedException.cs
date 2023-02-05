using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace ASCommander.DllInject.Exceptions
{
    internal class ProcessAcessDeniedException : Exception
    {
        public ProcessAcessDeniedException()
        {
        }

        public ProcessAcessDeniedException(string message) : base(message)
        {
        }

        public ProcessAcessDeniedException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected ProcessAcessDeniedException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}
