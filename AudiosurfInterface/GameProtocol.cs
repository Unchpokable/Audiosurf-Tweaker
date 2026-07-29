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
        public const string Minimize = "minimize";
        public const string Maximize = "maximize";
        public const string GoFullscreen = "gofullscreen";
        public const string RestoreNormalWindowStyle = "restorenormalwindowstyle";

        // Intercepted client-side (kills the game process directly, never actually sent as an
        // ascommand) - kept here anyway so the button's CommandParameter and TweakerViewModel.Send's
        // special-case match against the same literal instead of two hand-synced copies.
        public const string CloseAudiosurf = "closeaudiosurf";

        // NOTE: registerlistenerwindow / quickstartregisterwindow / quickstartqueuecommand are
        // deliberately NOT exposed publicly here. asbridge.exe already sends one of the first two
        // itself once it finds the game window (see Фаза 3c) - a second, managed-side registration
        // right alongside that first one would race/conflict with it. AudiosurfHandle.Command's own
        // queue-until-Connected behavior already gives callers the same practical effect as
        // quickstartqueuecommand.
        //
        // RegisterListenerWindow below is the one exception: it's internal, used only by
        // AudiosurfHandle's registration-ack watchdog to retry with the plain (non-quickstart)
        // command after the bridge's own attempt goes unanswered - at that point there's no longer a
        // race, since the bridge's attempt has already provably failed.
        internal const string RegisterListenerWindow = "registerlistenerwindow";

        // asreport names (broadcast content forwarded via AudiosurfHandle.MessageResieved, already
        // stripped of the "asreport" prefix by AsBridgeProtocol - see HandleGameBroadcast).
        public const string ReportSongComplete = "songcomplete";
        public const string ReportOnCharacterScreen = "oncharacterscreen";
        public const string ReportNowPlayingArtist = "nowplayingartistname";
        public const string ReportNowPlayingTitle = "nowplayingsongtitle";
        public const string ReportNowPlayingAshFile = "nowplayingashfile";

        public static string Config(string key, bool value) => $"{AsConfigPrefix} {key} {value.ToString().ToLower()}";

        public static string Command(string name) => $"{AsCommandPrefix} {name}";

        public static string SetWindowPositionSize(int x, int y, int width, int height) =>
            Command($"setwindowpositionsize {x},{y},{width},{height}");

        // GameCharacter.ToString().ToLowerInvariant() matches the command suffix exactly for every
        // value (DoubleVisionElite -> "doublevisionelite" -> playsongdoublevisionelite,
        // CurrentCharacter -> "currentcharacter" -> playsongcurrentcharacter) - no lookup table needed.
        public static string PlaySong(GameCharacter character, string filePath) =>
            Command($"playsong{character.ToString().ToLowerInvariant()} {filePath}");
    }
}
