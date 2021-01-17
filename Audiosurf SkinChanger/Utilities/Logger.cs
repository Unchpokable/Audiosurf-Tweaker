namespace Audiosurf_SkinChanger.Utilities
{
    using System;
    using System.IO;
    using System.Text;

    public class Logger
    {
        private string LogFilePath;

        public Logger()
        {
            LogFilePath = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + @"\Roaming\Audiosurf SkinChanger Logs";
        }

        public Logger(string path)
        {
            LogFilePath = path;
        }

        public void Log(string message)
        {
            Stream logStream;

            if (File.Exists(LogFilePath))
                logStream = new FileStream(LogFilePath, FileMode.Append);
            else
                logStream = new FileStream(LogFilePath, FileMode.Create);

            using (var writer = new StreamWriter(logStream, Encoding.UTF8))
            {
                writer.WriteLine(message);
            }

            logStream.Close();
            logStream.Dispose();
        }
    }
}
