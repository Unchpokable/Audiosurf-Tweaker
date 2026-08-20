using System;
using System.IO;
using System.Text;

namespace TweakerUI.Core
{
    /// <summary>
    /// The application's single log file. Static rather than instantiated per call site: every caller
    /// wrote to the same path anyway, and only one of the nine instances was ever wired to
    /// <see cref="ReadWriteException"/> - the other eight failed silently.
    /// </summary>
    public static class Logger
    {
        public static event Action<Exception> ReadWriteException;

        private static readonly string LogFilePath =
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "Audiosurf Tweaker Logs", "log.txt");

        public static void Log(string logTitle, string message)
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(LogFilePath));

                using (var writer = new StreamWriter(LogFilePath, append: true, Encoding.UTF8))
                    writer.WriteLine($"[{DateTime.Now}]::[{logTitle}]\n{message}\n");
            }
            catch (Exception e)
            {
                // Logger is typically called from inside another class's own catch block to record the
                // original failure - letting a narrower exception type (e.g. UnauthorizedAccessException
                // on a restricted MyDocuments path) escape here would replace that original error with an
                // unrelated crash instead.
                ReadWriteException?.Invoke(e);
            }
        }
    }
}
