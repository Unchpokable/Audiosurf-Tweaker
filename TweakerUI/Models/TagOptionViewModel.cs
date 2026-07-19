using System;
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using QuickPlayerCore.Audiosurf;

namespace TweakerUI.Models
{
    /// <summary>
    /// One row in a track's right-click "Tags" section - a toggle for a SongTagCatalog entry, plus
    /// an optional parameter text field for tags like [as-msz&lt;X&gt;]. Fires onChanged on every edit
    /// so the owning TrackCardViewModel can rebuild PlaylistEntry.Tags from the whole set rather than
    /// tracking incremental add/remove itself.
    /// </summary>
    public partial class TagOptionViewModel : ObservableObject
    {
        public TagOptionViewModel(SongTagDefinition definition, Action onChanged)
        {
            Definition = definition;
            _onChanged = onChanged;
        }

        public SongTagDefinition Definition { get; }
        public string Label => Definition.Description;
        public bool HasParameter => Definition.HasParameter;
        public string BracketPreview => Definition.HasParameter ? null : Definition.Format();

        [ObservableProperty]
        private bool isEnabled;

        [ObservableProperty]
        private string parameterText;

        private readonly Action _onChanged;

        public int? ParsedParameter => int.TryParse(ParameterText, out var value) ? value : null;

        partial void OnIsEnabledChanged(bool value) => _onChanged?.Invoke();

        partial void OnParameterTextChanged(string value) => _onChanged?.Invoke();

        // Tags with an AsConfigBinding (Sidewinder/BankingCamera) are live asconfig toggles the
        // Tweaker tab already exposes, not just a bracket marker in the title - they're presented as
        // Mods instead (see ModOptionViewModel), not duplicated here under the "Tags" styling.
        public static ObservableCollection<TagOptionViewModel> BuildCatalog(Action onChanged)
        {
            var options = new ObservableCollection<TagOptionViewModel>();
            foreach (var definition in SongTagCatalog.All)
            {
                if (definition.AsConfigBinding is null)
                    options.Add(new TagOptionViewModel(definition, onChanged));
            }
            return options;
        }
    }
}
