namespace TweakerUI.Core
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
        internal static bool IsDarkTheme { get; set; }
        // Whether the in-game plugin is driven from here at all: skins, tweaks and the Player tab in the
        // overlay (Docs/Internal/plugin-offline-mode.md §6.5). Off leaves an installed plugin running offline -
        // skyboxes and scripts keep working, the host just never talks to it.
        internal static bool SyncOverlayWithTweaker { get; set; }
        // The editor Script Dev opens projects in, by TweakerCore.Scripting.ScriptEditor.Id; empty until one is
        // chosen (script-manager.md §8.3).
        internal static string ScriptEditor { get; set; }
        // The new-project form's "Set up for AI agents" and the agents chosen last time (dev-bundle-roadmap.md §2.16):
        // whatever the user picked when they last set a project up is what the next one starts with.
        internal static bool ScriptAgentSetup { get; set; }
        internal static string ScriptAgents { get; set; }
        internal static bool ScriptAgentBinaries { get; set; }

        internal static string WatcherDefaultTemp => @"Storage\temp.tasp";
    }
}
