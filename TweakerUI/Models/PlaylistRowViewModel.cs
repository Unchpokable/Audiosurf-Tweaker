using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using QuickPlayerCore;

namespace TweakerUI.Models
{
    /// <summary>
    /// Wraps a Playlist for the sidebar list - Playlist itself is a plain persistence POCO (like
    /// PlaylistEntry), it has no INotifyPropertyChanged and rename-in-progress state has no business
    /// being serialized into the playlist's own JSON. Same role/rename UX as SkinCard wrapping
    /// AudiosurfSkinExtended (EnableRename/CancelRename/ApplyRename, TextBox swapped in for the
    /// name TextBlock while renaming).
    /// </summary>
    public partial class PlaylistRowViewModel : ObservableObject
    {
        public PlaylistRowViewModel(Playlist playlist)
        {
            Playlist = playlist;
            name = playlist.Name;
            playlist.Entries.CollectionChanged += (_, _) => OnPropertyChanged(nameof(TrackCount));
        }

        public Playlist Playlist { get; }

        public int TrackCount => Playlist.Entries.Count;

        [ObservableProperty]
        private string name;

        [ObservableProperty]
        private bool renameActive;

        // Pre-filled with the current name when entering rename mode, same reasoning as
        // SkinCard.NewName - it reads as "editing a name", not "typing one from scratch".
        [ObservableProperty]
        private string newName;

        [RelayCommand]
        private void EnableRename()
        {
            NewName = Name;
            RenameActive = true;
        }

        [RelayCommand]
        private void CancelRename() => RenameActive = false;

        [RelayCommand]
        private void ApplyRename()
        {
            RenameActive = false;

            if (string.IsNullOrWhiteSpace(NewName) || NewName == Name)
                return;

            Name = NewName;
            Playlist.Name = NewName;
            Playlist.Save();
        }
    }
}
