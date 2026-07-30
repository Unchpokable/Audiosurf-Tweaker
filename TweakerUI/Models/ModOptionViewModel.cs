using System;
using System.Collections.ObjectModel;
using System.Linq;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;

namespace TweakerUI.Models
{
    /// <summary>
    /// One row in a track's right-click "Mods" section - a per-song override for one of the same 7
    /// asconfig tweak keys the Tweaker tab exposes (TweakerViewModel), same labels/values. Sidewinder
    /// and Banking Camera are included here even though they're also listed in GameProtocol_SongTags.md
    /// as tag keywords ([as-swind]/[as-bankcam]) - they're genuine live asconfig toggles the Tweaker
    /// tab already lets the user flip, not just a title-string marker like the rest of SongTagCatalog,
    /// so they belong with the other mods, not styled as if they were plain bracket tags. Excluded
    /// from TagOptionViewModel.BuildCatalog for exactly this reason (see its own comment).
    /// </summary>
    public sealed class ModOptionDefinition
    {
        public ModOptionDefinition(string configKey, string label, bool valueWhenEnabled)
        {
            ConfigKey = configKey;
            Label = label;
            ValueWhenEnabled = valueWhenEnabled;
        }

        public string ConfigKey { get; }
        public string Label { get; }
        public bool ValueWhenEnabled { get; }
    }

    public partial class ModOptionViewModel : ObservableObject
    {
        public ModOptionViewModel(ModOptionDefinition definition, Action onChanged)
        {
            Definition = definition;
            _onChanged = onChanged;
        }

        public ModOptionDefinition Definition { get; }
        public string Label => Definition.Label;

        [ObservableProperty]
        private bool isEnabled;

        private readonly Action _onChanged;

        partial void OnIsEnabledChanged(bool value) => _onChanged?.Invoke();

        private static readonly ModOptionDefinition[] _catalog = GameTweakCatalog.All
            .Select(definition => new ModOptionDefinition(
                definition.ConfigKey,
                definition.DisplayName,
                definition.ConfigValueWhenEnabled))
            .ToArray();

        public static ObservableCollection<ModOptionViewModel> BuildCatalog(Action onChanged)
        {
            var options = new ObservableCollection<ModOptionViewModel>();
            foreach (var definition in _catalog)
                options.Add(new ModOptionViewModel(definition, onChanged));
            return options;
        }
    }
}
