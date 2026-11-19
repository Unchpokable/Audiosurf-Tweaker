using Microsoft.Win32;
using System;
using System.IO;
using System.Linq;
using TweakerCore.Configuration;
using Settings = TweakerUI.Core.SettingsProvider;

namespace TweakerUI.Core
{

    internal static class ConfigurationManager
    {
        internal static event Action<Exception> InitializationFaultCallback;

        // The apphost path is valid for both managed development and Native AOT publication.
        private static string ConfigPath => Environment.ProcessPath + ".config";

        private const string FirstRunKey = "FirstRun";

        // The single source of truth for every <appSettings> key: its default and how it maps onto
        // SettingsProvider in both directions. Three parallel lists of the same keys used to live here
        // (defaults, read, write) - a key added to one and forgotten in another silently stopped
        // persisting, with nothing to point at why.
        //
        // Missing keys are supplemented on disk without replacing the user's existing values.
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
            Bind("DarkTheme", false, () => Settings.IsDarkTheme, v => Settings.IsDarkTheme = v),
            Bind("SyncOverlayWithTweaker", true, () => Settings.SyncOverlayWithTweaker, v => Settings.SyncOverlayWithTweaker = v),
            Bind("ScriptEditor", "", () => Settings.ScriptEditor, v => Settings.ScriptEditor = v),
            Bind("ScriptAgentSetup", false, () => Settings.ScriptAgentSetup, v => Settings.ScriptAgentSetup = v),
            Bind("ScriptAgents", "claude-code,codex,cursor", () => Settings.ScriptAgents, v => Settings.ScriptAgents = v),
            Bind("ScriptAgentBinaries", false, () => Settings.ScriptAgentBinaries, v => Settings.ScriptAgentBinaries = v),
        };

        // Keys that used to be written here and no longer are. Their values are not carried anywhere - the
        // settings they stood for were replaced outright, not renamed - they are only swept out so a config
        // file does not keep accumulating rows nothing reads. Safe to empty once no user config still has them.
        private static readonly string[] ObsoleteKeys =
        {
            "UWPNotificationsAllowed",
            "UWPNotificationSilent",
            "EnableInGameOverlay", // -> SyncOverlayWithTweaker (plugin-offline-mode.md §6.5)
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
                var cfg = new XmlSettingsStore(ConfigPath);

                EnsureDefaultKeysExist(cfg);

                if (!bool.Parse(cfg.Get(FirstRunKey)) && Directory.Exists(cfg.Get("TexturesPath")))
                    return;

                cfg.Set(FirstRunKey, bool.FalseString);
                cfg.Save();

                var gameInstallPath = GetAudiosurfBaseDirectory();
                var texturesPath = $@"{gameInstallPath}\engine\textures";

                if (string.IsNullOrEmpty(gameInstallPath)
                    || !Directory.Exists(texturesPath))
                {
                    InitializationFaultCallback?.Invoke(new Exception("Can not detect audiosurf installation"));
                    return;
                }

                cfg.Set("TexturesPath", texturesPath);
                cfg.Save();
            }
            catch (Exception e)
            {
                InitializationFaultCallback?.Invoke(e);
            }
        }

        private static void EnsureDefaultKeysExist(XmlSettingsStore cfg)
        {
            var changed = cfg.Ensure(FirstRunKey, "true");
            foreach (var binding in Bindings)
                changed |= cfg.Ensure(binding.Key, binding.Default);

            foreach (var key in ObsoleteKeys)
                changed |= cfg.Remove(key);

            if (changed)
                cfg.Save();
        }

        public static void InitializeEnvironment()
        {
            try
            {
                var settings = new XmlSettingsStore(ConfigPath);

                foreach (var binding in Bindings)
                    binding.Write(settings.Get(binding.Key) ?? binding.Default);
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
                var cfg = new XmlSettingsStore(ConfigPath);
                EnsureDefaultKeysExist(cfg);

                foreach (var binding in Bindings)
                    cfg.Set(binding.Key, binding.Read());

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
            var libraryPath = GetAudiosurfBaseDirectoryFromVDF(libfolders);
            return libraryPath == null ? null : Path.Combine(libraryPath, "steamapps", "common", "Audiosurf");
        }

        private static string GetAudiosurfBaseDirectoryFromVDF(string libraryFoldersFilePath)
        {
            return SteamLibraryFolders.AudiosurfLibraries(File.ReadAllText(libraryFoldersFilePath)).FirstOrDefault();
        }
    }
}
