using System;
using System.Configuration;
using Microsoft.Win32;
using System.IO;
using Settings = SkinChangerRestyle.Core.SettingsProvider;
using System.Linq;
using Gameloop.Vdf;
using System.Collections.Generic;
using Gameloop.Vdf.JsonConverter;
using Windows.Foundation.Collections;
using Newtonsoft.Json;

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
                
                if (!bool.Parse(cfg.AppSettings.Settings["FirstRun"].Value))
                    return;

                cfg.AppSettings.Settings["FirstRun"].Value = bool.FalseString;
                cfg.Save();

                var gameInstallPath = GetAudiosurfBaseDirectory();
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
                Settings.InfopanelFontColor = System.Configuration.ConfigurationManager.AppSettings.Get("InfopanelFontColor");
                Settings.InfopanelFontSize = System.Configuration.ConfigurationManager.AppSettings.Get("InfopanelFontSize");
                Settings.InfopanelXOffset = System.Configuration.ConfigurationManager.AppSettings.Get("InfopanelXOffset");
                Settings.InfopanelYOffset = System.Configuration.ConfigurationManager.AppSettings.Get("InfopanelYOffset");
                Settings.InstalledServerPackageName = System.Configuration.ConfigurationManager.AppSettings.Get("InstalledServerPackageName");
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
                cfg.AppSettings.Settings["InfopanelFontColor"].Value = Settings.InfopanelFontColor;
                cfg.AppSettings.Settings["InfopanelFontSize"].Value = Settings.InfopanelFontSize;
                cfg.AppSettings.Settings["InfopanelXOffset"].Value = Settings.InfopanelXOffset;
                cfg.AppSettings.Settings["InfopanelYOffset"].Value = Settings.InfopanelYOffset;
                cfg.AppSettings.Settings["InstalledServerPackageName"].Value = Settings.InstalledServerPackageName;
                cfg.Save();
            }
            catch (Exception e)
            {
                InitializationFaultCallback?.Invoke(e);
                return;
            }
        }

        public static bool UpdateSection(string key, string value)
        {
            try
            {
                var cfg = System.Configuration.ConfigurationManager.OpenExeConfiguration(AppDomain.CurrentDomain.FriendlyName);

                if (cfg.AppSettings.Settings.AllKeys.Contains(key))
                {
                    cfg.AppSettings.Settings[key].Value = value;
                    cfg.Save();
                    return true;
                }
                return false;
            }
            catch
            {
                return false;
            }
        }

        private static string GetSteamInstallPath()
        {
            using (var steamKey = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam"))
            {
                if (steamKey != null)
                {
                    return steamKey.GetValue("SteamPath") as string;
                }
            }

            return null;
        }

        private static string GetAudiosurfBaseDirectory()
        {
            var steamPath = GetSteamInstallPath().Replace("/", "\\");
            if (steamPath == null)
            {
                return null;
            }

            var libfolders = Path.Combine(steamPath, "steamapps", "libraryfolders.vdf");

            if (!File.Exists(libfolders))
            {
                var steamapps = Path.Combine(steamPath, "steamapps\\common");

                foreach (var directory in Directory.EnumerateDirectories(steamapps))
                {
                    if (directory.EndsWith("Audiosurf"))
                        return directory;
                }
                return null;
            }
            else 
                return Path.Combine(GetAudiosurfBaseDirectoryFromVDF(libfolders), "steamapps\\common\\Audiosurf");
        }

        private static string GetAudiosurfBaseDirectoryFromVDF(string libfordersFilePath)
        {
            var vdf = VdfConvert.Deserialize(File.ReadAllText(libfordersFilePath));
            var json = JsonConvert.DeserializeObject<Dictionary<string, LibraryFoldersRecord>>(vdf.ToJson().Value.ToString());
            foreach (var folder in json.Keys)
            {
                if (json[folder].Apps.ContainsKey("12900"))
                    return json[folder].Path;
            }

            return null;
        }

        private class LibraryFoldersRecord
        {
            public string Path { get; set; }
            public string Label { get; set; }
            public string ContentId { get; set; }
            public string TotalSize { get; set; }
            public string UpdateCleanBytesTally { get; set; }
            public string TimeLastUpdateCorruption { get; set; }
            public Dictionary<string, string> Apps { get; set; }
        }
    }
}
