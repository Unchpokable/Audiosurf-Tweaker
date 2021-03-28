using System.Linq;
using System;
using System.IO.Compression;

namespace Installer
{
    class ZipUnpacker
    {
        public static bool Check(string path, string[] programFiles)
        {
            using (var filesInZip = ZipFile.OpenRead(path))
            {
                bool flag = filesInZip.Entries.Any(x => programFiles.Contains(x.Name));
                return flag;
            }
        }

        public static bool Unpack(string archivePath, string installPath)
        {
            try
            {
                ZipFile.ExtractToDirectory(archivePath, installPath);
                return true;
            }
            catch (Exception e)
            {
                Console.WriteLine($"[Error] :: Installation Error: {e.Message}");
                return false;
            }
        }
    }
}
