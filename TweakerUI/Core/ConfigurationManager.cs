using Gameloop.Vdf;
using Gameloop.Vdf.JsonConverter;
using Microsoft.Win32;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.IO;
using Settings = TweakerUI.Core.SettingsProvider;

namespace TweakerUI.Core
{

    internal static class ConfigurationManager
    {
        internal static event Action<Exception> InitializationFaultCallback;

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

        private const string FirstRunKey = "FirstRun";

        // The single source of truth for every <appSettings> key: its default and how it maps onto
        // SettingsProvider in both directions. Three parallel lists of the same keys used to live here
        // (defaults, read, write) - a key added to one and forgotten in another silently stopped
        // persisting, with nothing to point at why.
        //
        // The defaults exist because OpenExeConfiguration on a missing/incomplete config file returns a
        // non-null but empty AppSettings section (confirmed by direct test), so Settings[key].Value threw
        // a NullReferenceException that SetUpDefaultSettings' own try/catch swallowed *before* it ever
        // reached cfg.Save() - meaning a missing config file could never self-heal, it would just fail the
        // same way on every launch. EnsureDefaultKeysExist closes that regardless of why a key is missing
        // (packaging step, manual edit, a setting added after a user's config was already on disk, ...).
        private static readonly SettingBinding[] Bindings =
        {
            Bind("TexturesPath", "None", () => Settings.GameTexturesPath, v => Settings.GameTexturesPath = v),
            Bind("AddSkinsPath", "None", () => Settings.SkinsFolderPath, v => Settings.SkinsFolderPath = v),
            Bind("HotReload", true, () => Settings.HotReload, v => Settings.HotReload = v),
            Bind("DCSActive", true, () => Settings.ControlSystemActive, v => Settings.ControlSystemActive = v),
            Bind("SafeInstall", false, () => Settings.SafeInstall, v => Settings.SafeInstall = v),
            Bind("UseFastPreview", false, () => Settings.UseFastPreview, v => Settings.UseFastPreview = v),
            Bind("WatcherEnabled", false, () => Settings.WatcherEnabled, v => Settings.WatcherEnabled = v),
            Bind("WatcherTempFile", "Storage/temp.tasp", () => Settings.WatcherTempFile, v => Settings.WatcherTempFile = v),
            Bind("WatcherShouldStoreTextures", false, () => Settings.WatcherShouldStoreTextures, v => Settings.WatcherShouldStoreTextures = v),
            Bind("WatcherTempFileOverrided", false, () => Settings.WatcherTempFileOverrided, v => Settings.WatcherTempFileOverrided = v),
            Bind("UWPNotificationsAllowed", false, () => Settings.IsUWPNotificationsAllowed, v => Settings.IsUWPNotificationsAllowed = v),
            Bind("UWPNotificationSilent", true, () => Settings.IsUWPNotificationSilent, v => Settings.IsUWPNotificationSilent = v),
            Bind("DarkTheme", false, () => Settings.IsDarkTheme, v => Settings.IsDarkTheme = v),
            Bind("EnableInGameOverlay", false, () => Settings.EnableInGameOverlay, v => Settings.EnableInGameOverlay = v),
        };

        private sealed record SettingBinding(string Key, string Default, Func<string> Read, Action<string> Write);

        private static SettingBinding Bind(string key, string defaultValue, Func<string> read, Action<string> write) =>
            new SettingBinding(key, defaultValue, read, write);

        private static SettingBinding Bind(string key, bool defaultValue, Func<bool> read, Action<bool> write) =>
            new SettingBinding(key, defaultValue ? "true" : "false", () => read().ToString(), value => write(bool.Parse(value)));

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

                if (!bool.Parse(cfg.AppSettings.Settings[FirstRunKey].Value) && Directory.Exists(cfg.AppSettings.Settings["TexturesPath"].Value))
                    return;

                cfg.AppSettings.Settings[FirstRunKey].Value = bool.FalseString;
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

            void EnsureKey(string key, string defaultValue)
            {
                if (cfg.AppSettings.Settings[key] != null)
                    return;

                cfg.AppSettings.Settings.Add(key, defaultValue);
                addedAnyKey = true;
            }

            EnsureKey(FirstRunKey, "true");
            foreach (var binding in Bindings)
                EnsureKey(binding.Key, binding.Default);

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

                foreach (var binding in Bindings)
                    binding.Write(settings[binding.Key]?.Value);
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
                var cfg = System.Configuration.ConfigurationManager.OpenExeConfiguration(ExePath);

                foreach (var binding in Bindings)
                    cfg.AppSettings.Settings[binding.Key].Value = binding.Read();

                cfg.Save();
            }
            catch (Exception e)
            {
                InitializationFaultCallback?.Invoke(e);
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

        // Only the two fields this lookup needs; Json.NET ignores the rest of the record.
        private class LibraryFoldersRecord
        {
            public string Path { get; set; }
            public Dictionary<string, string> Apps { get; set; }
        }
    }
}
