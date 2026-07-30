using System;
using System.Collections.Generic;
using System.Linq;

namespace AudiosurfInterface
{
    /// <summary>
    /// The seven user-facing tweaks shared by the desktop UI, Quick Player and TW_OVL. A definition
    /// owns the conversion between the human "tweak enabled" value and the game's raw asconfig value,
    /// so inverted options cannot drift between those consumers.
    /// </summary>
    public sealed class GameTweakDefinition
    {
        internal GameTweakDefinition(string configKey, string wireName, string displayName, bool configValueWhenEnabled)
        {
            ConfigKey = configKey;
            WireName = wireName;
            DisplayName = displayName;
            ConfigValueWhenEnabled = configValueWhenEnabled;
        }

        public string ConfigKey { get; }
        public string WireName { get; }
        public string DisplayName { get; }
        public bool ConfigValueWhenEnabled { get; }

        public bool DefaultConfigValue => !ConfigValueWhenEnabled;

        public bool ToConfigValue(bool enabled) => enabled ? ConfigValueWhenEnabled : !ConfigValueWhenEnabled;

        public bool ToEnabledValue(bool configValue) => configValue == ConfigValueWhenEnabled;
    }

    public static class GameTweakCatalog
    {
        public static IReadOnlyList<GameTweakDefinition> All { get; } = new[]
        {
            new GameTweakDefinition(GameProtocol.RoadVisible, "InvisibleRoad", "Invisible Road", false),
            new GameTweakDefinition(GameProtocol.ShowSongName, "HiddenSongTitle", "Hidden Song Title", false),
            new GameTweakDefinition(GameProtocol.Sidewinder, "SidewinderCamera", "Sidewinder camera", true),
            new GameTweakDefinition(GameProtocol.UseBankingCamera, "BankingCamera", "Banking camera", true),
            new GameTweakDefinition(GameProtocol.FreerideBlocks, "FreerideNoBlocks", "Freeride: No Blocks", false),
            new GameTweakDefinition(
                GameProtocol.FreerideCaterpillars,
                "FreerideBlocksCaterpillars",
                "Freeride: Blocks Caterpillars",
                true),
            new GameTweakDefinition(
                GameProtocol.FreerideAutoAdvance,
                "FreerideAutoAdvanceDisable",
                "Freeride: Disable Auto Advance",
                false),
        };

        private static readonly Dictionary<string, GameTweakDefinition> _byConfigKey =
            All.ToDictionary(definition => definition.ConfigKey, StringComparer.Ordinal);

        private static readonly Dictionary<string, GameTweakDefinition> _byWireName =
            All.ToDictionary(definition => definition.WireName, StringComparer.Ordinal);

        public static GameTweakDefinition FindByConfigKey(string configKey) =>
            configKey != null && _byConfigKey.TryGetValue(configKey, out var definition) ? definition : null;

        public static GameTweakDefinition FindByWireName(string wireName) =>
            wireName != null && _byWireName.TryGetValue(wireName, out var definition) ? definition : null;

        public static bool DefaultConfigValue(string configKey) => FindByConfigKey(configKey)?.DefaultConfigValue ?? false;
    }
}
