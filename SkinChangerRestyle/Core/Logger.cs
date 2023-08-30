using System;
using System.IO;
using System.Text;

namespace SkinChangerRestyle.Core
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
            catch (IOException e)
            {
                ReadWriteException?.Invoke(this, new UnhandledExceptionEventArgs(e, false));
            }
        }

        private string FormatMessage(string logTitle, string message)
        {
            return $"[{DateTime.Now}]::[{logTitle}]\n{message}\n";
        }
    }
}
