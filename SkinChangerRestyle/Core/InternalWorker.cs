namespace SkinChangerRestyle.Core
{
    using System;
    using System.Configuration;
    using Microsoft.Win32;
    using System.IO;
    using Env = SkinChangerRestyle.Core.Environment;

    internal static class InternalWorker
    {
        public static void SetUpDefaultSettings()
        {
            Configuration cfg = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);
            if (cfg.AppSettings.Settings["FirstRun"].Value != "Yes")
                return;

            string pathToAudiosurfTextures;
            if (System.Environment.Is64BitOperatingSystem)
            {
                pathToAudiosurfTextures = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath", null).ToString();
            }
            else
            {
                pathToAudiosurfTextures = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam", "InstallPath", null).ToString();
            }

            if (string.IsNullOrEmpty(pathToAudiosurfTextures) || string.IsNullOrWhiteSpace(pathToAudiosurfTextures))
                //MessageBox.Show("Ooops! Audiosurf Skin Changer can't find your steam! So Please, select path to audiosurf textues manually", "Path Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);

            if (!Directory.Exists(pathToAudiosurfTextures + @"\steamapps\common\Audiosurf"))
                /*MessageBox.Show(
                    $@"Ooops! Audiosurf Skin Changer can't find your Audiosurf!
                    This can happen if you use an illegal or just non-steam copy of the game. 
                    So Please, select path to audiosurf textues manually", "Path Error", 
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);*/

            cfg.AppSettings.Settings["FirstRun"].Value = "no";
            cfg.AppSettings.Settings["gamePath"].Value = pathToAudiosurfTextures + @"\steamapps\common\Audiosurf\engine\textures";
            cfg.Save();
            ConfigurationManager.RefreshSection("appSettings");
        }

        public static void InitializeEnvironment()
        {
            Env.gamePath = ConfigurationManager.AppSettings.Get("gamePath");
            Env.skinsFolderPath = ConfigurationManager.AppSettings.Get("skinsPath");
            Env.ControlSystemBehaviour = ParseBehaviourFromConfig();
            Env.DCSWarningsAllowed = ParseIsWarningsAllowedFromConfig();
        }

        private static DCSBehaviour ParseBehaviourFromConfig()
        {
            var currentValue = ConfigurationManager.AppSettings.Get("DCSBehaviour");
            switch (currentValue)
            {
                case "0":
                    return DCSBehaviour.OnBoot;
                case "1":
                    return DCSBehaviour.AsyncAfterBoot;
                default:
                    throw new Exception("Wrong Configuration parameter");
            }
        }

        private static bool ParseIsWarningsAllowedFromConfig()
        {
            return bool.Parse(ConfigurationManager.AppSettings.Get("AllowWarnings"));
        }
    }
}
