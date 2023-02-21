using System;
using System.Configuration;
using Microsoft.Win32;
using System.IO;
using Settings = SkinChangerRestyle.Core.SettingsProvider;


namespace SkinChangerRestyle.Core
{

    internal delegate void ExternExceptionHandler(Exception innerException);

    internal static class ConfigurationManager
    {
        internal static event ExternExceptionHandler InitializationFaultCallback;

        public static void SetUpDefaultSettings()
        {
            try
            {
                Configuration cfg = System.Configuration.ConfigurationManager.OpenExeConfiguration(AppDomain.CurrentDomain.FriendlyName);

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
                System.Configuration.ConfigurationManager.RefreshSection("appSettings");
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
                Settings.GameTexturesPath = System.Configuration.ConfigurationManager.AppSettings.Get("TexturesPath");
                Settings.SkinsFolderPath = System.Configuration.ConfigurationManager.AppSettings.Get("AddSkinsPath");
                Settings.ControlSystemActive = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("DCSActive"));
                Settings.HotReload = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("HotReload"));
                Settings.SafeInstall = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("SafeInstall"));
                Settings.WatcherTempFile = System.Configuration.ConfigurationManager.AppSettings.Get("WatcherTempFile");
                Settings.WatcherShouldStoreTextures = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("WatcherShouldStoreTextures"));
                Settings.WatcherTempFileOverrided = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("WatcherTempFileOverrided"));
                Settings.WatcherEnabled = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("WatcherEnabled"));
                Settings.UseFastPreview = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("UseFastPreview"));
                Settings.IsUWPNotificationsAllowed = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("UWPNotificationsAllowed"));
                Settings.IsUWPNotificationSilent = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("UWPNotificationSilent"));
                Settings.IsOverlayEnabled = bool.Parse(System.Configuration.ConfigurationManager.AppSettings.Get("OverlayEnabled"));
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
                Configuration cfg = System.Configuration.ConfigurationManager.OpenExeConfiguration(AppDomain.CurrentDomain.FriendlyName);
                cfg.AppSettings.Settings["TexturesPath"].Value = Settings.GameTexturesPath;
                cfg.AppSettings.Settings["AddSkinsPath"].Value = Settings.SkinsFolderPath;
                cfg.AppSettings.Settings["HotReload"].Value = Settings.HotReload.ToString();
                cfg.AppSettings.Settings["DCSActive"].Value = Settings.ControlSystemActive.ToString();
                cfg.AppSettings.Settings["SafeInstall"].Value = Settings.SafeInstall.ToString();
                cfg.AppSettings.Settings["WatcherTempFile"].Value = Settings.WatcherTempFile;
                cfg.AppSettings.Settings["WatcherShouldStoreTextures"].Value = Settings.WatcherShouldStoreTextures.ToString();
                cfg.AppSettings.Settings["WatcherTempFileOverrided"].Value = Settings.WatcherTempFileOverrided.ToString();
                cfg.AppSettings.Settings["WatcherEnabled"].Value = Settings.WatcherEnabled.ToString();
                cfg.AppSettings.Settings["UseFastPreview"].Value = Settings.UseFastPreview.ToString();
                cfg.AppSettings.Settings["UWPNotificationsAllowed"].Value = Settings.IsUWPNotificationsAllowed.ToString();
                cfg.AppSettings.Settings["UWPNotificationSilent"].Value = Settings.IsUWPNotificationSilent.ToString();
                cfg.AppSettings.Settings["OverlayEnabled"].Value = Settings.IsOverlayEnabled.ToString();
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
