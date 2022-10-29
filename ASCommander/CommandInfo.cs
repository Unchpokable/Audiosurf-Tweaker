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
