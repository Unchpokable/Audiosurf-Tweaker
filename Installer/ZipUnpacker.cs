using System.Linq;
using System.IO.Compression;

namespace Installer
{
    class ZipUnpacker
    {
        public static bool Check(string path, string[] programFiles)
        {
            var filesInZip = ZipFile.OpenRead(path);
            return filesInZip.Entries.Any(x => programFiles.Contains(x.Name));
        }

        public static bool Unpack(string archivePath, string installPath)
        {
            try
            {
                ZipFile.ExtractToDirectory(archivePath, installPath);
                return true;
            }
            catch (System.Exception e)
            {
                System.Console.WriteLine($"[Error] :: Installation Error: {e.Message}");
                return false;
            }
        }
    }
}
