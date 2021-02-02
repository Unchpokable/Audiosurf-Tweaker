using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Collections.Generic;
using System.Collections;
using System.Diagnostics;
using static System.Net.Mime.MediaTypeNames;

namespace Installer
{
    class Program
    {
        internal static readonly string[] programFiles = new[]
        {
            "icon.ico",
            "askinicon.ico",
            "Audiosurf SkinChanger.exe",
            "Audiosurf SkinChanger.exe.config"
        };

        public static void Main(string[] args)
         {
            Console.WriteLine("Welcome to \"SkinChanger Installation script\"");
            string whereIAm = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            Console.WriteLine($"[Info] :: Current location: {whereIAm}");
            Console.WriteLine("[Info] :: Checking that Audiosurf Skin Changer files exists...");
            string sourceArchive = FindAudiosurfSkinChangerZip(whereIAm);

            if (sourceArchive == null)
            {
                Console.WriteLine("[Installation Abort]");
                Console.WriteLine("[Error] :: Couldn't find Audiosurf Skin Changer Files");
                return;
            }

            if (!ZipUnpacker.Check(sourceArchive, programFiles))
            {
                Console.WriteLine($"[Error] :: Archive {sourceArchive} doesn't contains Audiosurf Skin Changer files and no other archive founded in {whereIAm}");
                return;
            }

            Console.WriteLine("Installing Audiosurf Skin Changer...");
            bool isOk = ZipUnpacker.Unpack(sourceArchive, whereIAm);
            if (isOk)
                Console.WriteLine($"[Info] :: SkinChanger Successfully installed. Registering .askin files icon...");
            else
                return;

            if (!IconRegistry.RegisterIcon("askin", whereIAm + @"\askinicon.ico"))
                Console.WriteLine("[Warn] :: Icon registration Error");
            else
            {
                Console.WriteLine("[Info] :: Successfully installed Audiosurf skinchanger. Thanks for download. Enjoy");
                IconRegistry.SHChangeNotify(0x08000000, 0x0000, (IntPtr)null, (IntPtr)null);
            }
            Console.WriteLine("Press any key to continue...");
            Console.ReadKey();
            File.Delete(sourceArchive);
            Process.Start(new ProcessStartInfo()
            {
                Arguments = "/C choice /C Y /N /D Y /T 3 & Del \"" + Assembly.GetExecutingAssembly().Location + "\"",
                WindowStyle = ProcessWindowStyle.Hidden,
                CreateNoWindow = true,
                FileName = "cmd.exe"
            });
        }

        internal static string FindAudiosurfSkinChangerZip(string location)
        {
            string[] allFiles = Directory.GetFiles(location).Where(x => x.EndsWith(".zip")).ToArray();
            string mask = "audiosurf";
            foreach(var path in allFiles)
            {
                if (Path.GetFileName(path).ToLower().Contains(mask))
                    return path;
            }
            return null;
        }
    }
}
