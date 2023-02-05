using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;

namespace ASCommander.DllInject.Exceptions
{
    internal class GenericInjectionException : Exception
    {
        public GenericInjectionException()
        {
        }

        public GenericInjectionException(string message) : base(message)
        {
        }

        public GenericInjectionException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected GenericInjectionException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}
