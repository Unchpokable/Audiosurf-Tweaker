﻿namespace SkinChangerRestyle.Core
{
    using System;
    using System.Configuration;
    using Microsoft.Win32;
    using System.IO;
    using Settings = SkinChangerRestyle.Core.SettingsProvider;

    internal delegate void ExternExceptionHandler(Exception innerException);

    internal static class InternalWorker
    {
        internal static ExternExceptionHandler InitializationFaultCallback { private get; set; }
        internal static string FingerPrint;


        public static void SetUpDefaultSettings()
        {
            Configuration cfg = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);

            if (!bool.Parse(cfg.AppSettings.Settings["FirstRun"].Value))
                return;

            var pathToAudiosurfTextures = Environment.Is64BitOperatingSystem
                                          ? Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath", null).ToString()
                                          : Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Valve\Steam", "InstallPath", null).ToString();

            if (string.IsNullOrEmpty(pathToAudiosurfTextures) 
                || string.IsNullOrWhiteSpace(pathToAudiosurfTextures)
                || !Directory.Exists(pathToAudiosurfTextures + @"\steamapps\common\Audiosurf"))
            {
                InitializationFaultCallback?.Invoke(new Exception("Couldn't find Audiosurf pathes or registry access denied by operating system"));
                return;
            }

            cfg.AppSettings.Settings["TexturesPath"].Value = pathToAudiosurfTextures + @"\steamapps\common\Audiosurf\engine\textures";
            cfg.Save();
            ConfigurationManager.RefreshSection("appSettings");
        }

        public static void InitializeEnvironment()
        {
            try
            {
                Settings.GameTexturesPath = ConfigurationManager.AppSettings.Get("TexturesPath");
                Settings.SkinsFolderPath = ConfigurationManager.AppSettings.Get("AddSkinsPath");
                Settings.ControlSystemActive = bool.Parse(ConfigurationManager.AppSettings.Get("DCSActive"));
                Settings.HotReload = bool.Parse(ConfigurationManager.AppSettings.Get("HotReload"));
            }
            catch (Exception e)
            {
                InitializationFaultCallback.Invoke(e);
                return;
            }
        }

        public static void RewriteSettings()
        {
            try
            {
                Configuration cfg = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);
                cfg.AppSettings.Settings["TexturesPath"].Value = Settings.GameTexturesPath;
                cfg.AppSettings.Settings["AddSkinsPath"].Value = Settings.SkinsFolderPath;
                cfg.AppSettings.Settings["HotReload"].Value = Settings.HotReload.ToString();
                cfg.AppSettings.Settings["DCSActive"].Value = Settings.ControlSystemActive.ToString();
            }
            catch (Exception e)
            {
                InitializationFaultCallback.Invoke(e);
                return;
            }
        }
    }
}
