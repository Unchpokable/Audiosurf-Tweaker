using System;
using System.Configuration;
using Microsoft.Win32;


namespace Audiosurf_SkinChanger.Engine
{
    public static class InternalWorker
    {
        public static void SetUpDefaultSettings()
        {
            Configuration cfg = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);
            if (cfg.AppSettings.Settings["FirstRun"].Value != "Yes")
                return;

            string pathToAudiosurfTextures;
            if (Environment.Is64BitOperatingSystem)
            {
                pathToAudiosurfTextures = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath", null) + @"\steamapps\common\Audiosurf\engine\textures";
            }
            else
            {
                pathToAudiosurfTextures = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam", "InstallPath", null) + @"\steamapps\common\Audiosurf\engine\textures";
            }

            cfg.AppSettings.Settings["FirstRun"].Value = "no";
            cfg.AppSettings.Settings["gamePath"].Value = pathToAudiosurfTextures;
            cfg.Save();
            ConfigurationManager.RefreshSection("appSettings");
        }

        public static void InitializeEnvironment()
        {
            EnvironmentalVeriables.gamePath = ConfigurationManager.AppSettings.Get("gamePath");
            EnvironmentalVeriables.skinsFolderPath = ConfigurationManager.AppSettings.Get("skinsPath");
        }
    }
}
