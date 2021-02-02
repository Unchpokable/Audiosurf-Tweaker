using Microsoft.Win32;
using System;
using System.Runtime.InteropServices;

namespace Installer
{
    class IconRegistry
    {
        public static bool RegisterIcon(string extension, string pathToIcon)
        {
            try
            {
                if (Registry.ClassesRoot.GetValue("." + extension) == null)
                {
                    return RegisterNew(extension, pathToIcon);
                }
                else
                {
                    return UpdateOld(extension, pathToIcon);
                }
            }
            catch (Exception e)
            {
                Console.WriteLine($"[Error] :: Failed to registry icon: {pathToIcon}");
                Console.WriteLine($"[Error] :: Instalation Failture: {e.Message}");
                return false;
            }
        }

        private static bool RegisterNew(string extension, string pathToIcon)
        {
            RegistryKey key = Registry.ClassesRoot.CreateSubKey("." + extension); //create ".ext" registry key
            key.SetValue("", extension);
            RegistryKey iconKey = Registry.ClassesRoot.CreateSubKey(extension); //create "ext" reg key to register default icon
            RegistryKey defIcon = iconKey.CreateSubKey("DefaultIcon");
            defIcon.SetValue("", pathToIcon);
            return true;
        }

        private static bool UpdateOld(string extension, string pathToIcon)
        {
            try
            {
                RegistryKey reg = Registry.CurrentUser.OpenSubKey(extension);
                reg.SetValue("", pathToIcon);
                return true;
            }
            catch (Exception e)
            {
                Console.WriteLine($"[Error] :: Failed to registry icon: {pathToIcon}");
                Console.WriteLine($"[Error] :: Instalation Failture: {e.Message}");
                return false;
            }
        }

        [DllImport("Shell32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern void SHChangeNotify(int eventID, uint uFlags, IntPtr dwItem1, IntPtr dwItem2);
    }
}
