namespace SkinChangerRestyle.Core
{
    using System;
    using System.Configuration;
    using Microsoft.Win32;
    using System.IO;
    using Settings = SkinChangerRestyle.Core.SettingsProvider;

    internal delegate void ExternExceptionHandler(Exception innerException);

    internal static class InternalWorker
    {
        internal static event ExternExceptionHandler InitializationFaultCallback;

        public static void SetUpDefaultSettings()
        {
            try
            {
                Configuration cfg = ConfigurationManager.OpenExeConfiguration(AppDomain.CurrentDomain.FriendlyName);

                if (cfg.AppSettings == null)
                {
                    InitializationFaultCallback?.Invoke(new Exception("Null configuration section"));
                    return;
                }
                var SurfRegistryPath = @"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 12900";

                if (!bool.Parse(cfg.AppSettings.Settings["FirstRun"].Value))
                    return;

                cfg.AppSettings.Settings["FirstRun"].Value = bool.FalseString;
                cfg.Save();

                var gameInstallPath = Registry.GetValue(SurfRegistryPath, "InstallLocation", null)?.ToString();
                var texturesPath = $@"{gameInstallPath}\engine\textures";

                if (string.IsNullOrEmpty(gameInstallPath)
                    || !Directory.Exists(texturesPath))
                {
                    InitializationFaultCallback?.Invoke(new Exception("Can not detect audiosurf installation"));
                    return;
                }

                cfg.AppSettings.Settings["TexturesPath"].Value = texturesPath;
                cfg.Save();
                ConfigurationManager.RefreshSection("appSettings");
            }
            catch (Exception e)
            {
                InitializationFaultCallback?.Invoke(e);
            }
        }

        public static void InitializeEnvironment()
        {
            try
            {
                Settings.GameTexturesPath = ConfigurationManager.AppSettings.Get("TexturesPath");
                Settings.SkinsFolderPath = ConfigurationManager.AppSettings.Get("AddSkinsPath");
                Settings.ControlSystemActive = bool.Parse(ConfigurationManager.AppSettings.Get("DCSActive"));
                Settings.HotReload = bool.Parse(ConfigurationManager.AppSettings.Get("HotReload"));
                Settings.SafeInstall = bool.Parse(ConfigurationManager.AppSettings.Get("SafeInstall"));
            }
            catch (Exception e)
            {
                InitializationFaultCallback?.Invoke(e);
                return;
            }
        }

        public static void RewriteSettings()
        {
            try
            {
                Configuration cfg = ConfigurationManager.OpenExeConfiguration(AppDomain.CurrentDomain.FriendlyName);
                cfg.AppSettings.Settings["TexturesPath"].Value = Settings.GameTexturesPath;
                cfg.AppSettings.Settings["AddSkinsPath"].Value = Settings.SkinsFolderPath;
                cfg.AppSettings.Settings["HotReload"].Value = Settings.HotReload.ToString();
                cfg.AppSettings.Settings["DCSActive"].Value = Settings.ControlSystemActive.ToString();
                cfg.AppSettings.Settings["SafeInstall"].Value = Settings.SafeInstall.ToString();
                cfg.Save();
            }
            catch (Exception e)
            {
                InitializationFaultCallback?.Invoke(e);
                return;
            }
        }
    }
}
