using System;
using System.IO;
using System.Text;

namespace TweakerUI.Core
{

    public class Logger
    {
        public UnhandledExceptionEventHandler ReadWriteException;

        private string LogFilePath;

        public Logger()
        {
            LogFilePath = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments) + @"\Audiosurf Tweaker Logs\log.txt";
        }

        public Logger(string path)
        {
            LogFilePath = path;
        }

        public void Log(string logTitle, string message)
        {
            try
            {
                if (!File.Exists(LogFilePath))
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(LogFilePath));
                    using (var _ = File.Create(LogFilePath)) { }
                }

                using (var logStream = new FileStream(LogFilePath, FileMode.Append))
                using (var writer = new StreamWriter(logStream, Encoding.UTF8))
                {
                    writer.WriteLine(FormatMessage(logTitle, message));
                }
            }
            catch (Exception e)
            {
                // Logger is typically called from inside another class's own catch block to record the
                // original failure - letting a narrower exception type (e.g. UnauthorizedAccessException
                // on a restricted MyDocuments path) escape here would replace that original error with an
                // unrelated crash instead.
                ReadWriteException?.Invoke(this, new UnhandledExceptionEventArgs(e, false));
            }
        }

        private string FormatMessage(string logTitle, string message)
        {
            return $"[{DateTime.Now}]::[{logTitle}]\n{message}\n";
        }
    }
}
