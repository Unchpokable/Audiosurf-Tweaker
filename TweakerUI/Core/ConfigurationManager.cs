using Gameloop.Vdf;
using Gameloop.Vdf.JsonConverter;
using Microsoft.Win32;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.IO;
using System.Linq;
using Settings = TweakerUI.Core.SettingsProvider;

namespace TweakerUI.Core
{

    internal delegate void ExternExceptionHandler(Exception innerException);

    internal static class ConfigurationManager
    {
        internal static event ExternExceptionHandler InitializationFaultCallback;

        // OpenExeConfiguration(path) looks for "<path>.config". AppDomain.CurrentDomain.FriendlyName
        // was "audiosurftweaker.exe" on .NET Framework (matching the shipped audiosurftweaker.exe.config),
        // but on modern .NET it's just "audiosurftweaker" with no extension - which doesn't match the
        // config file the SDK actually produces, so every read/write here silently missed the real
        // config. Previously fixed with Assembly.GetExecutingAssembly().Location (giving
        // TweakerUI.dll.config, which the SDK does produce on a normal multi-file publish) - but that
        // broke again under Deploy.ps1's self-contained single-file publish: Assembly.Location returns
        // an empty string for assemblies loaded from a PublishSingleFile bundle (documented .NET
        // behavior, no separate .dll on disk to report a path for), so every OpenExeConfiguration call
        // below silently failed and every setting silently sat on its C# default - same failure class
        // as the FriendlyName bug this comment used to describe, just re-triggered by a different publish
        // mode. Environment.ProcessPath (the actual apphost .exe, not the managed assembly) resolves
        // correctly in every publish mode - framework-dependent, self-contained multi-file, and
        // single-file alike - and gives TweakerUI.exe.config, which OpenExeConfiguration creates fresh
        // via the self-healing path below (EnsureDefaultKeysExist) if it isn't there yet.
        private static string ExePath => Environment.ProcessPath;

        // Mirrors the <appSettings> keys shipped in App.config. Read by EnsureDefaultKeysExist below -
        // OpenExeConfiguration on a missing/incomplete config file returns a non-null but empty
        // AppSettings section (confirmed by direct test), so Settings["FirstRun"].Value threw a
        // NullReferenceException that SetUpDefaultSettings' own try/catch swallowed *before* it ever
        // reached cfg.Save() - meaning a missing config file could never self-heal, it would just fail
        // the same way on every single launch. This keeps that failure mode from being silent/permanent
        // regardless of why the file's missing or incomplete (packaging step, manually deleted key, a
        // setting added after a user's config was already on disk, ...).
        private static readonly (string Key, string Default)[] DefaultAppSettings =
        {
            ("FirstRun", "true"),
            ("TexturesPath", "None"),
            ("AddSkinsPath", "None"),
            ("HotReload", "true"),
            ("DCSActive", "true"),
            ("SafeInstall", "false"),
            ("UseFastPreview", "false"),
            ("WatcherEnabled", "false"),
            ("WatcherTempFile", "Storage/temp.tasp"),
            ("WatcherShouldStoreTextures", "false"),
            ("WatcherTempFileOverrided", "false"),
            ("UWPNotificationsAllowed", "false"),
            ("UWPNotificationSilent", "true"),
            ("DarkTheme", "false"),
            ("EnableInGameOverlay", "false"),
        };

        public static void SetUpDefaultSettings()
        {
            try
            {
                Configuration cfg = System.Configuration.ConfigurationManager.OpenExeConfiguration(ExePath);

                if (cfg.AppSettings == null)
                {
                    InitializationFaultCallback?.Invoke(new Exception("Null configuration section"));
                    return;
                }

                EnsureDefaultKeysExist(cfg);

                if (!bool.Parse(cfg.AppSettings.Settings["FirstRun"].Value) && Directory.Exists(cfg.AppSettings.Settings["TexturesPath"].Value))
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

        private static void EnsureDefaultKeysExist(Configuration cfg)
        {
            var addedAnyKey = false;
            foreach (var (key, defaultValue) in DefaultAppSettings)
            {
                if (cfg.AppSettings.Settings[key] != null)
                    continue;

                cfg.AppSettings.Settings.Add(key, defaultValue);
                addedAnyKey = true;
            }

            if (addedAnyKey)
                cfg.Save();
        }

        public static void InitializeEnvironment()
        {
            try
            {
                // Explicit OpenExeConfiguration(ExePath), not the static implicit ConfigurationManager.AppSettings -
                // the implicit resolution mirrors the same FriendlyName-based lookup that's broken on modern .NET
                // (see ExePath comment above), so it misses the real config file the same way.
                var settings = System.Configuration.ConfigurationManager.OpenExeConfiguration(ExePath).AppSettings.Settings;
                string Get(string key) => settings[key]?.Value;

                Settings.GameTexturesPath = Get("TexturesPath");
                Settings.SkinsFolderPath = Get("AddSkinsPath");
                Settings.ControlSystemActive = bool.Parse(Get("DCSActive"));
                Settings.HotReload = bool.Parse(Get("HotReload"));
                Settings.SafeInstall = bool.Parse(Get("SafeInstall"));
                Settings.WatcherTempFile = Get("WatcherTempFile");
                Settings.WatcherShouldStoreTextures = bool.Parse(Get("WatcherShouldStoreTextures"));
                Settings.WatcherTempFileOverrided = bool.Parse(Get("WatcherTempFileOverrided"));
                Settings.WatcherEnabled = bool.Parse(Get("WatcherEnabled"));
                Settings.UseFastPreview = bool.Parse(Get("UseFastPreview"));
                Settings.IsUWPNotificationsAllowed = bool.Parse(Get("UWPNotificationsAllowed"));
                Settings.IsUWPNotificationSilent = bool.Parse(Get("UWPNotificationSilent"));
                Settings.IsDarkTheme = bool.Parse(Get("DarkTheme"));
                Settings.EnableInGameOverlay = bool.Parse(Get("EnableInGameOverlay"));
            }
            catch (Exception e)
            {
                InitializationFaultCallback?.Invoke(e);
            }
        }

        public static void RewriteSettings()
        {
            try
            {
                Configuration cfg = System.Configuration.ConfigurationManager.OpenExeConfiguration(ExePath);
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
                cfg.AppSettings.Settings["DarkTheme"].Value = Settings.IsDarkTheme.ToString();
                cfg.AppSettings.Settings["EnableInGameOverlay"].Value = Settings.EnableInGameOverlay.ToString();
                cfg.Save();
            }
            catch (Exception e)
            {
                InitializationFaultCallback?.Invoke(e);
            }
        }

        public static bool UpdateSection(string key, string value)
        {
            try
            {
                var cfg = System.Configuration.ConfigurationManager.OpenExeConfiguration(ExePath);

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
            var steamPath = GetSteamInstallPath()?.Replace("/", "\\");

            if (steamPath == null)
                return string.Empty;

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
