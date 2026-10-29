using System.IO;
using TweakerCore.PluginInstall;

namespace TweakerCore.Skyboxes
{
    /// <summary>
    /// Where the Skybox Replacer keeps things in the game (Docs/skyboxes/getting-started.md, "Where things live";
    /// the plugin's src/plugin/paths):
    ///
    ///     engine\TweakerStuff\SkyboxReplacer\
    ///       module.json        the module's own settings - enabled, the chosen sky (SkyModuleConfig)
    ///       Skyboxes\          the skies: .sky packages (a folder or its zip), loose images, folders of faces, .hlsl
    ///       SkyConfigs\        what the player moved on each sky; the host never touches it
    /// </summary>
    public static class SkyPaths
    {
        public const string ReplacerFolderName = "SkyboxReplacer";
        public const string SkyboxesFolderName = "Skyboxes";
        public const string ModuleConfigFileName = "module.json";

        /// <summary>A package's extension, for the folder form and the zip alike.</summary>
        public const string PackageExtension = ".sky";

        /// <summary>Deploy-manifest folder of the bundled skies: "TweakerStuff/SkyboxReplacer/Skyboxes/".</summary>
        public const string SkyboxesRelativeFolder = PluginInstallation.StuffFolderName + "/" + ReplacerFolderName + "/" + SkyboxesFolderName + "/";

        public static string ReplacerPath(string engineDirectory)
        {
            return Path.Combine(engineDirectory, PluginInstallation.StuffFolderName, ReplacerFolderName);
        }

        public static string SkyboxesPath(string engineDirectory)
        {
            return Path.Combine(ReplacerPath(engineDirectory), SkyboxesFolderName);
        }

        public static string ModuleConfigPath(string engineDirectory)
        {
            return Path.Combine(ReplacerPath(engineDirectory), ModuleConfigFileName);
        }

        /// <summary>engine\TweakerStuff\ - there only when the plugin was installed; without it nothing is installed into the game.</summary>
        public static bool HasPluginFolder(string engineDirectory)
        {
            return !string.IsNullOrEmpty(engineDirectory) && Directory.Exists(Path.Combine(engineDirectory, PluginInstallation.StuffFolderName));
        }
    }
}
