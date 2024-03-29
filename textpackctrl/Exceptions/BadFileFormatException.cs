﻿using System;
using System.Runtime.Serialization;

namespace FolderChecker.Exceptions
{

    class BadFileFormatException : Exception
    {
        public BadFileFormatException(string message) : base(message)
        {
        }

        public BadFileFormatException(string message, Exception innerException) : base(message, innerException)
        {
        }

        protected BadFileFormatException(SerializationInfo info, StreamingContext context) : base(info, context)
        {
        }
    }
}
