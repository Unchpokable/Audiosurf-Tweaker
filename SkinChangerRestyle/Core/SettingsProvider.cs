namespace SkinChangerRestyle.Core
{
    using ChangerAPI.Engine;
    using System.Collections.Generic;
    using System.IO;

    internal static class SettingsProvider
    {
        internal static string SkinsFolderPath { get; set; }
        internal static string GameTexturesPath { get; set; }
        internal static bool ControlSystemActive { get; set; }
        internal static bool HotReload { get; set; }
        internal static bool SafeInstall { get; set; }

        internal static string DefaultSkinsPath
        {
            get
            {
                return Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location) + @"\\Skins";
            }
        }
    }
}
