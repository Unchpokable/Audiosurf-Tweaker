using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using QuickPlayerCore;

namespace TweakerUI.Models
{
    /// <summary>
    /// One chip in the transport bar's playback-mode row. Same callback shape as
    /// CharacterOptionViewModel: the option knows its own value and calls back into the owner, so the
    /// DataTemplate never has to reach up through the visual tree.
    ///
    /// A roster rather than one bool property per mode (as the two-state Advance mode row does): with
    /// six mutually exclusive values that pattern is six near-identical properties, and each of them
    /// needs the "ignore the group's unchecking write" guard.
    /// </summary>
    public partial class PlaybackModeOptionViewModel : ObservableObject
    {
        public PlaybackModeOptionViewModel(PlaybackMode value, string displayName, string hint, Action<PlaybackMode> onSelected)
        {
            Value = value;
            DisplayName = displayName;
            Hint = hint;
            _onSelected = onSelected;
        }

        public PlaybackMode Value { get; }
        public string DisplayName { get; }

        /// <summary>Tooltip text - the labels distinguish the modes but do not explain them.</summary>
        public string Hint { get; }

        [ObservableProperty]
        private bool isSelected;

        private readonly Action<PlaybackMode> _onSelected;

        [RelayCommand]
        private void Select() => _onSelected?.Invoke(Value);

        // Linear modes first, then the shuffled ones, each group going from "least repeating" outwards -
        // the row reads as a scale rather than as six unrelated buttons.
        private static readonly (PlaybackMode Value, string DisplayName, string Hint)[] _roster =
        {
            (PlaybackMode.Single, "Single",
                "Plays the track you start and stops. Next and Prev still work."),
            (PlaybackMode.Sequential, "In order",
                "Plays the playlist top to bottom and stops after the last track."),
            (PlaybackMode.RepeatOne, "Repeat one",
                "Starts the same track again every time it finishes. Next and Prev still move on."),
            (PlaybackMode.RepeatAll, "Repeat all",
                "Plays the playlist top to bottom, then starts it over."),
            (PlaybackMode.Shuffle, "Shuffle",
                "One pass over the playlist in a random order, then stops."),
            (PlaybackMode.ShuffleLoop, "Shuffle loop",
                "Random order, reshuffled into a fresh pass every time it runs out.")
        };

        public static ObservableCollection<PlaybackModeOptionViewModel> BuildRoster(Action<PlaybackMode> onSelected)
        {
            var roster = new ObservableCollection<PlaybackModeOptionViewModel>();
            foreach (var (value, displayName, hint) in _roster)
                roster.Add(new PlaybackModeOptionViewModel(value, displayName, hint, onSelected));
            return roster;
        }

        public static void SelectOnly(IEnumerable<PlaybackModeOptionViewModel> roster, PlaybackMode selected)
        {
            foreach (var option in roster)
                option.IsSelected = option.Value == selected;
        }
    }
}
