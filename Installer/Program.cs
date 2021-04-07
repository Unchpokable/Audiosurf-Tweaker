using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Diagnostics;
using IWshRuntimeLibrary;
using System.Runtime.InteropServices.ComTypes;

namespace Installer
{
    class Program
    {
        internal static readonly string[] programFiles = new[]
        {
            "icon.ico",
            "askinicon.ico",
            "Audiosurf SkinChanger.exe",
            "Audiosurf SkinChanger.exe.config",
            "foldercontrol.dll"
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
                IconRegistry.SHChangeNotify(0x08000000, 0x0000, (IntPtr)null, (IntPtr)null);
                Console.WriteLine("[Info] :: Icons successfully registered. Audisourf Skin Changer ready to use");
            }

            Console.WriteLine("Do you want to create a shortcut on your desktop? Y/N");
            if (Console.ReadLine().ToLower() == "y")
                CreateShortcut();
            Console.WriteLine("Audiosurf Skin Changer succesfully installed. Thanks for download. Enjoy");
            Console.WriteLine("Press any key to continue...");
            Console.ReadKey();
            System.IO.File.Delete(sourceArchive);
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
            string[] allFiles = Directory.GetFiles(location).Where(x => x.EndsWith(".prog")).ToArray();
            string mask = "audiosurf";
            foreach(var path in allFiles)
            {
                if (Path.GetFileName(path).ToLower().Contains(mask))
                    return path;
            }
            return null;
        }

        internal static void CreateShortcut()
        {
            IShellLink link = (IShellLink)new ShellLink();
            link.SetDescription("Audiosurf Skin Changer Shortcut");
            var folder = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            link.SetPath(folder + @"\Audiosurf SkinChanger.exe");
            link.SetWorkingDirectory(folder);

            var file = (IPersistFile)link;
            var desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
            file.Save(Path.Combine(desktopPath, "Audiosurf Skin Changer.lnk"), false);
        }
    }
}
