using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerUI.Core;

namespace TweakerUI.Models
{
    /// <summary>
    /// One button in a 14-character grid (transport bar's global default, or a track's per-song
    /// override). IsSelected is driven externally by whichever owner holds the "selected character"
    /// value; clicking fires the Select command (self-contained, no parameter binding needed since
    /// each option already knows its own Value) which calls back into the owner - same callback
    /// pattern as TagOptionViewModel/ModOptionViewModel, avoids reaching up through the visual tree
    /// from inside an ItemsControl's DataTemplate.
    /// </summary>
    public partial class CharacterOptionViewModel : ObservableObject
    {
        public CharacterOptionViewModel(GameCharacter value, string displayName, Action<GameCharacter> onSelected)
        {
            Value = value;
            DisplayName = displayName;
            _onSelected = onSelected;
        }

        public GameCharacter Value { get; }
        public string DisplayName { get; }

        [ObservableProperty]
        private bool isSelected;

        private readonly Action<GameCharacter> _onSelected;

        [RelayCommand]
        private void Select() => _onSelected?.Invoke(Value);

        public static ObservableCollection<CharacterOptionViewModel> BuildRoster(Action<GameCharacter> onSelected)
        {
            var roster = new ObservableCollection<CharacterOptionViewModel>();
            foreach (var (value, displayName) in CharacterRoster.RealCharacters)
                roster.Add(new CharacterOptionViewModel(value, displayName, onSelected));
            return roster;
        }

        public static void SelectOnly(IEnumerable<CharacterOptionViewModel> roster, GameCharacter selected)
        {
            foreach (var option in roster)
                option.IsSelected = option.Value == selected;
        }
    }
}
