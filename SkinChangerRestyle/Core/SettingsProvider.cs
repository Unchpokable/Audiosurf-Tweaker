using System.IO;

namespace SkinChangerRestyle.Core
{

    internal static class SettingsProvider
    {
        internal static string SkinsFolderPath { get; set; }
        internal static string GameTexturesPath { get; set; }
        internal static bool ControlSystemActive { get; set; }
        internal static bool HotReload { get; set; }
        internal static bool SafeInstall { get; set; }
        internal static bool UseFastPreview { get; set; }
        internal static bool WatcherEnabled { get; set; }
        internal static string WatcherTempFile { get; set; }
        internal static bool WatcherShouldStoreTextures { get; set; }
        internal static bool WatcherTempFileOverrided { get; set; }

        internal static bool IsUWPNotificationsAllowed { get; set; }
        internal static bool IsUWPNotificationSilent { get; set; }
        internal static bool IsOverlayEnabled { get; set; }
        internal static bool IsOverlayInstanceAlive { get; set; } = false; // false by default. No one can call overlay while this flag set to false

        internal static string DefaultSkinsPath => Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location) + @"\\Skins";
        internal static string WatcherDefaultTemp => @"storage\temp.tasp";
    }
}
