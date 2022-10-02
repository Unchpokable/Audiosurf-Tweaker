using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ASCommander
{
    public class CommandInfo
    {
        public enum CommandStatus
        {
            Sent,
            Enqueued
        }

        public CommandInfo(string text, CommandStatus status)
        {
            CommandText = text;
            Status = status;
        }

        public readonly string CommandText;
        public readonly CommandStatus Status;
    }
}
