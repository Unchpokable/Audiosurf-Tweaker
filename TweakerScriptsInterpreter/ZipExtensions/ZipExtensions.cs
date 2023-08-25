using System.IO;
using System.IO.Compression;

namespace TweakerScriptsInterpreter.ZipExtensions
{
    internal static class ZipExtensions
    {
        public static void ExtractForced(this ZipArchive zip, string destination)
        {
            if (!Directory.Exists(destination))
                Directory.CreateDirectory(destination);

            foreach (var entry in zip.Entries)
            {
                var destinationPath = Path.GetFullPath(Path.Combine(destination, entry.FullName));

                if (entry.Length == 0)
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(destinationPath));
                    continue;
                }

                entry.ExtractToFile(destinationPath, true);
            }
        }
    }
}
