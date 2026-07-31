using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using QuickPlayerCore;
using TweakerUI.Core;
using TweakerUI.Models;
using TweakerUI.Services;

namespace TweakerUI.ViewModels
{
    /// <summary>
    /// QuickPlayer tab (Фаза 5) - a playlist sidebar, the selected playlist's queue, and a transport
    /// bar (prev/play/next + the 14-character default-mode grid). Owns the single PlaybackController
    /// for the app; EntryStarted/EntryEnded drive both the queue's IsPlaying highlighting and the
    /// app-wide status bar (StatusService) - the wiring TempFileTagger/PlaybackController were built
    /// UI-agnostic for, see QuickPlayerCore's "Расположение кода" notes.
    /// </summary>
    public partial class QuickPlayerViewModel : ViewModelBase
    {
        public QuickPlayerViewModel()
        {
            DefaultCharacterOptions = CharacterOptionViewModel.BuildRoster(character => DefaultCharacter = character);
            CharacterOptionViewModel.SelectOnly(DefaultCharacterOptions, DefaultCharacter);

            _playback = new PlaybackController { DefaultCharacter = DefaultCharacter };
            _playback.EntryPreparing += OnEntryPreparing;
            _playback.EntryPrepared += OnEntryPrepared;
            _playback.EntryStarted += OnEntryStarted;
            _playback.EntryEnded += OnEntryEnded;
            // Auto-advance starts the next track off the report thread, so a failure there has no
            // caller to surface it - without this it would just stop playing with no explanation.
            _playback.OperationFailed += OnPlaybackFailed;

            foreach (var playlist in Playlist.LoadAll())
                Playlists.Add(new PlaylistRowViewModel(playlist));

            SelectedPlaylistRow = Playlists.FirstOrDefault();
        }

        public ObservableCollection<PlaylistRowViewModel> Playlists { get; } = new();
        public ObservableCollection<TrackCardViewModel> Queue { get; } = new();
        public ObservableCollection<CharacterOptionViewModel> DefaultCharacterOptions { get; }

        [ObservableProperty]
        private PlaylistRowViewModel selectedPlaylistRow;

        [ObservableProperty]
        private TrackCardViewModel selectedTrackCard;

        [ObservableProperty]
        private TrackCardViewModel currentTrackCard;

        [ObservableProperty]
        private GameCharacter defaultCharacter = CharacterRoster.RealCharacters[0].Value;

        private readonly PlaybackController _playback;
        private StatusHandle _nowPlayingStatus;

        // Preparing a track runs on a background thread in both directions (Task.Run for a manual play,
        // PlaybackController's own dispatch for auto-advance), so the chip is guarded by a lock -
        // StatusService itself already marshals the collection edit to the UI thread.
        private readonly object _preparingGate = new();
        private StatusHandle _preparingStatus;
        private int _preparingCount;

        // Raw Playlist for internal use - Playlists/SelectedPlaylistRow are the UI-facing wrappers
        // (rename state etc.), everything below just needs the underlying persisted model.
        private Playlist SelectedPlaylist => SelectedPlaylistRow?.Playlist;

        partial void OnSelectedPlaylistRowChanged(PlaylistRowViewModel value)
        {
            Queue.Clear();
            NotifyAdvanceModeChanged();
            if (value == null)
                return;

            foreach (var entry in value.Playlist.Entries)
                Queue.Add(new TrackCardViewModel(entry, this));
        }

        // Advance mode. Stored on the playlist rather than as an app-wide preference, next to
        // AutoAdvance - a mix playlist and a "sit and grind one chart" playlist genuinely want
        // different answers.
        //
        // Two properties instead of one bool with an inverting converter, because the view binds a
        // RadioButton pair: the group unchecks the other member by writing false into it, and a shared
        // bool would flip the very setting that click was choosing. Each setter therefore acts only on
        // being checked and ignores the unchecking write.
        public bool IsAdvanceAuto
        {
            get => SelectedPlaylist != null && SelectedPlaylist.AdvanceOn == AdvanceTrigger.SongComplete;
            set
            {
                if (value)
                    SetAdvanceTrigger(AdvanceTrigger.SongComplete);
            }
        }

        public bool IsAdvanceManual
        {
            get => SelectedPlaylist != null && SelectedPlaylist.AdvanceOn == AdvanceTrigger.CharacterScreen;
            set
            {
                if (value)
                    SetAdvanceTrigger(AdvanceTrigger.CharacterScreen);
            }
        }

        public string AdvanceAutoHint =>
            "The next track starts the moment the current one ends - the score screen goes by without a stop.";

        public string AdvanceManualHint =>
            "The next track waits until you leave the score screen, so you can read your result first. "
            + "Quitting a song early still stops playback either way.";

        private void SetAdvanceTrigger(AdvanceTrigger trigger)
        {
            var playlist = SelectedPlaylist;
            if (playlist == null || playlist.AdvanceOn == trigger)
                return;

            playlist.AdvanceOn = trigger;
            NotifyAdvanceModeChanged();
            SaveCurrentPlaylist();
        }

        private void NotifyAdvanceModeChanged()
        {
            OnPropertyChanged(nameof(IsAdvanceAuto));
            OnPropertyChanged(nameof(IsAdvanceManual));
        }

        partial void OnDefaultCharacterChanged(GameCharacter value)
        {
            CharacterOptionViewModel.SelectOnly(DefaultCharacterOptions, value);
            _playback.DefaultCharacter = value;
        }

        [RelayCommand]
        private void NewPlaylist()
        {
            var playlist = new Playlist { Name = "New Playlist" };
            playlist.Save();
            var row = new PlaylistRowViewModel(playlist);
            Playlists.Add(row);
            SelectedPlaylistRow = row;
        }

        [RelayCommand]
        private async Task RemovePlaylist()
        {
            if (SelectedPlaylistRow == null)
                return;

            if (!await ApplicationNotificationManager.Manager.AskForAction("Remove Playlist",
                    $"This will delete \"{SelectedPlaylistRow.Name}\" and its saved tags/mods. Are you sure?"))
                return;

            var removed = SelectedPlaylistRow;
            var index = Playlists.IndexOf(removed);
            removed.Playlist.Delete();
            Playlists.Remove(removed);
            SelectedPlaylistRow = Playlists.Count > 0 ? Playlists[Math.Min(index, Playlists.Count - 1)] : null;
        }

        [RelayCommand]
        private async Task AddTracks()
        {
            if (SelectedPlaylist == null)
                return;

            var files = await FileDialogService.OpenFilesAsync("Add tracks", BuildAudioFileTypeFilter());
            await AddFilesAsync(files);
        }

        // Shared by the "Add tracks" dialog and drag&drop onto the queue (QuickPlayerView.axaml.cs) -
        // same split as SkinChangerViewModel.HandleFileDrop/AddNewSkin.
        public async Task AddFilesAsync(IEnumerable<string> files)
        {
            if (SelectedPlaylist == null)
                return;

            var supported = SupportedAudioFormats.FilterSupported(files).ToList();
            if (supported.Count == 0)
                return;

            List<PlaylistEntry> entries;
            using (StatusService.Manager.Begin(StatusToken.DiskProcess, "Quick Player", $"Adding {supported.Count} track(s)..."))
                entries = await Task.Run(() => supported.Select(PlaylistEntry.FromFile).ToList());

            foreach (var entry in entries)
            {
                SelectedPlaylist.Entries.Add(entry);
                Queue.Add(new TrackCardViewModel(entry, this));
            }

            SaveCurrentPlaylist();
        }

        // EnsureConnected is checked first in every one of these, before any index math - Prev used
        // to fall through to a silent no-op when nothing was playing (its "no current track" index
        // fallback computed an already-invalid prevIndex), so unlike Next it never even reached the
        // connection check, which read as "Prev is just broken" rather than "not connected".
        // Checking connection unconditionally up front makes every transport button behave the same.
        //
        // PlaybackController.CurrentEntry is the queue *position*, not "what is audibly playing" - it
        // deliberately outlives the track, so Next/Prev keep their place after a song ends or the
        // player bails out to the character screen instead of jumping back to the top of the playlist.
        [RelayCommand]
        private Task PlaySelected()
        {
            if (!EnsureConnected())
                return Task.CompletedTask;

            var card = SelectedTrackCard ?? Queue.FirstOrDefault();
            return card != null ? PlayCard(card) : Task.CompletedTask;
        }

        [RelayCommand]
        private Task Next()
        {
            if (!EnsureConnected() || SelectedPlaylist == null)
                return Task.CompletedTask;

            var index = _playback.CurrentEntry != null ? SelectedPlaylist.Entries.IndexOf(_playback.CurrentEntry) : -1;
            var nextIndex = index + 1;
            return nextIndex < SelectedPlaylist.Entries.Count ? PlayIndexAsync(nextIndex) : Task.CompletedTask;
        }

        [RelayCommand]
        private Task Prev()
        {
            if (!EnsureConnected() || SelectedPlaylist == null)
                return Task.CompletedTask;

            var index = _playback.CurrentEntry != null ? SelectedPlaylist.Entries.IndexOf(_playback.CurrentEntry) : 0;
            var prevIndex = index - 1;
            return prevIndex >= 0 ? PlayIndexAsync(prevIndex) : Task.CompletedTask;
        }

        public Task PlayCard(TrackCardViewModel card)
        {
            if (!EnsureConnected() || SelectedPlaylist == null)
                return Task.CompletedTask;

            var index = SelectedPlaylist.Entries.IndexOf(card.Entry);
            return index >= 0 ? PlayIndexAsync(index) : Task.CompletedTask;
        }

        public void RemoveCard(TrackCardViewModel card)
        {
            if (SelectedPlaylist == null)
                return;

            SelectedPlaylist.Entries.Remove(card.Entry);
            Queue.Remove(card);
            SaveCurrentPlaylist();
        }

        public void SaveCurrentPlaylist() => SelectedPlaylist?.Save();

        // Sends gotocharacterscreen and resets the module to idle - the only way to interrupt a
        // song, since the game protocol has no pause and PlaybackController on its own only reacts
        // to the game telling *it* things ended, it never asks the game to stop anything.
        [RelayCommand]
        private void Stop()
        {
            AudiosurfHandle.Instance.Command(GameProtocol.Command(GameProtocol.GoToCharacterScreen));
            _playback.Stop();
        }

        // The "Preparing..." chip is raised by PlaybackController (EntryPreparing/EntryPrepared), not
        // here: auto-advance never goes through this method, and it is the path where the user has no
        // other sign the module is doing anything at all.
        private async Task PlayIndexAsync(int index)
        {
            if (!EnsureConnected())
                return;

            var playlist = SelectedPlaylist;
            await Task.Run(() => _playback.Play(playlist, index));
        }

        // Command()/GameConfigState.Set() queue silently and flush whenever the game does connect -
        // fine for background tweaks, but for "hit play" the user needs to know right away that
        // nothing is actually about to happen, not have it fire off minutes later when the game
        // finally loads.
        private static bool EnsureConnected()
        {
            if (AudiosurfHandle.Instance.IsValid)
                return true;

            ApplicationNotificationManager.Manager.ShowError("Not connected",
                "Audiosurf isn't connected yet - get to the character screen in-game before playing tracks.");
            return false;
        }

        private void OnEntryStarted(PlaylistEntry entry)
        {
            Dispatcher.UIThread.Post(() =>
            {
                _nowPlayingStatus?.Dispose();
                _nowPlayingStatus = StatusService.Manager.Begin(StatusToken.Playing, "Quick Player",
                    $"Now playing: {entry.ArtistName} - {entry.SongTitle}");

                if (CurrentTrackCard != null)
                    CurrentTrackCard.IsPlaying = false;

                CurrentTrackCard = Queue.FirstOrDefault(c => c.Entry.Id == entry.Id);
                if (CurrentTrackCard != null)
                    CurrentTrackCard.IsPlaying = true;
            });
        }

        // Counted rather than one-handle-per-event: a manual play can start while an auto-advance is
        // still preparing, and with a single slot the first Prepared to arrive would close the chip out
        // from under the preparation still running. One chip lives for as long as anything is preparing,
        // showing whatever started most recently.
        private void OnEntryPreparing(PlaylistEntry entry)
        {
            var message = $"Preparing {entry.SongTitle}...";
            lock (_preparingGate)
            {
                _preparingCount++;
                if (_preparingStatus == null)
                    _preparingStatus = StatusService.Manager.Begin(StatusToken.DiskProcess, "Quick Player", message);
                else
                    _preparingStatus.Update(message);
            }
        }

        private void OnEntryPrepared(PlaylistEntry entry)
        {
            StatusHandle finished = null;
            lock (_preparingGate)
            {
                if (--_preparingCount <= 0)
                {
                    _preparingCount = 0;
                    finished = _preparingStatus;
                    _preparingStatus = null;
                }
            }

            finished?.Dispose();
        }

        private void OnPlaybackFailed(string context, Exception exception)
        {
            Dispatcher.UIThread.Post(() =>
                ApplicationNotificationManager.Manager.ShowError("Quick Player", $"{context}: {exception.Message}"));
        }

        private void OnEntryEnded(PlaylistEntry entry)
        {
            Dispatcher.UIThread.Post(() =>
            {
                _nowPlayingStatus?.Dispose();
                _nowPlayingStatus = null;

                var card = Queue.FirstOrDefault(c => c.Entry.Id == entry.Id);
                if (card != null)
                    card.IsPlaying = false;
                if (CurrentTrackCard == card)
                    CurrentTrackCard = null;
            });
        }

        private static IReadOnlyList<FilePickerFileType> BuildAudioFileTypeFilter() => new[]
        {
            new FilePickerFileType("Audio files") { Patterns = SupportedAudioFormats.Extensions.Select(e => "*" + e).ToList() }
        };
    }
}
