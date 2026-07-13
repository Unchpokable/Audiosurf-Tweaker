namespace AudiosurfInterface
{
    /// <summary>
    /// Everything about the game's own ascommand/asconfig text protocol lives here - the literal
    /// command/config-key strings Audiosurf itself recognizes inside the WM_COPYDATA payload the bridge
    /// forwards (distinct from asbridge's own CCOMMAND/SREPORT wire format, see Bridge/AsBridgeProtocol),
    /// plus the two helpers that format a full command string from them. Centralized so a command name
    /// only has to be typed correctly once instead of being kept in sync by hand between C# and XAML.
    /// </summary>
    public static class GameProtocol
    {
        private const string AsConfigPrefix = "asconfig";
        private const string AsCommandPrefix = "ascommand";

        // asconfig keys (tweak toggles)
        public const string RoadVisible = "roadvisible";
        public const string ShowSongName = "showsongname";
        public const string Sidewinder = "sidewinder";
        public const string UseBankingCamera = "usebankingcamera";
        public const string FreerideBlocks = "freerideblocks";
        public const string FreerideCaterpillars = "freeridecaterpillars";
        public const string FreerideAutoAdvance = "freerideautoadvance";

        // ascommand names
        public const string SetWindowAlwaysOnTop = "setwindowalwaysontop";
        public const string SetWindowAlwaysOnTopNoBorder = "setwindowalwaysontopnoborder";
        public const string RunWithoutFocus = "runwithoutfocus";
        public const string ReloadTextures = "reloadtextures";
        public const string ReloadSounds = "reloadsounds";
        public const string GoToCharacterScreen = "gotocharacterscreen";
        public const string Maximize = "maximize";
        public const string GoFullscreen = "gofullscreen";
        public const string RestoreNormalWindowStyle = "restorenormalwindowstyle";

        // Intercepted client-side (kills the game process directly, never actually sent as an
        // ascommand) - kept here anyway so the button's CommandParameter and TweakerViewModel.Send's
        // special-case match against the same literal instead of two hand-synced copies.
        public const string CloseAudiosurf = "closeaudiosurf";

        public static string Config(string key, bool value) => $"{AsConfigPrefix} {key} {value.ToString().ToLower()}";

        public static string Command(string name) => $"{AsCommandPrefix} {name}";
    }
}
