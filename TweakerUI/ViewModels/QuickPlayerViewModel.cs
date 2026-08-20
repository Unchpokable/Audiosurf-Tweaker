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
    public partial class QuickPlayerViewModel : ViewModelBase, IDisposable
    {
        public QuickPlayerViewModel()
        {
            DefaultCharacterOptions = CharacterOptionViewModel.BuildRoster(character => DefaultCharacter = character);
            CharacterOptionViewModel.SelectOnly(DefaultCharacterOptions, DefaultCharacter);
            PlaybackModeOptions = PlaybackModeOptionViewModel.BuildRoster(SetPlaybackMode);

            _playback = new PlaybackController { DefaultCharacter = DefaultCharacter };
            _playback.EntryPreparing += OnEntryPreparing;
            _playback.EntryPrepared += OnEntryPrepared;
            _playback.EntryStarted += OnEntryStarted;
            _playback.EntryEnded += OnEntryEnded;
            // Auto-advance starts the next track off the report thread, so a failure there has no
            // caller to surface it - without this it would just stop playing with no explanation.
            _playback.OperationFailed += OnPlaybackFailed;
            _playback.EntryUnavailable += OnEntryUnavailable;
            _playback.PrewarmProgressed += OnPrewarmProgressed;

            foreach (var playlist in Playlist.LoadAll())
                Playlists.Add(new PlaylistRowViewModel(playlist));

            SelectedPlaylistRow = Playlists.FirstOrDefault();

            // Built last, so it sees a fully populated playlist list: it subscribes to the state
            // above rather than being driven by it, and would otherwise push an empty catalog to an
            // overlay that connected during construction.
            _overlayBridge = new QuickPlayerOverlayBridge(this, _playback);
        }

        /// <summary>
        /// A playlist's contents or settings changed. Raised from <see cref="SaveCurrentPlaylist"/>
        /// because every mutation already funnels through it - tags, mods, per-track character,
        /// adding and removing tracks, reordering, playback mode, advance trigger - so there is one
        /// place to hook rather than seven. Exists for the in-game overlay, which is push-based and
        /// cannot poll (see Docs/Internal/overlay-quickplayer.md).
        /// </summary>
        public event EventHandler<Playlist> PlaylistChanged;

        public ObservableCollection<PlaylistRowViewModel> Playlists { get; } = new();
        public ObservableCollection<TrackCardViewModel> Queue { get; } = new();
        public ObservableCollection<CharacterOptionViewModel> DefaultCharacterOptions { get; }
        public ObservableCollection<PlaybackModeOptionViewModel> PlaybackModeOptions { get; }

        [ObservableProperty]
        private PlaylistRowViewModel selectedPlaylistRow;

        [ObservableProperty]
        private TrackCardViewModel selectedTrackCard;

        [ObservableProperty]
        private TrackCardViewModel currentTrackCard;

        [ObservableProperty]
        private GameCharacter defaultCharacter = CharacterRoster.RealCharacters[0].Value;

        /// <summary>
        /// Whether the 14-character grid is open. It is closed by default: the grid is a once-a-session
        /// setting and it was permanently costing the queue - the only part of this page that scrolls -
        /// about 90px of height. The collapsed header states the current pick (see
        /// <see cref="DefaultCharacterName"/>), so nothing is actually hidden by this.
        /// </summary>
        [ObservableProperty]
        private bool isCharacterPickerExpanded;

        /// <summary>Current default character's label, for the collapsed picker's header row.</summary>
        public string DefaultCharacterName =>
            DefaultCharacterOptions.FirstOrDefault(option => option.Value == DefaultCharacter)?.DisplayName;

        private readonly PlaybackController _playback;
        private readonly QuickPlayerOverlayBridge _overlayBridge;
        private StatusHandle _nowPlayingStatus;

        // Preparing a track runs on a background thread in both directions (Task.Run for a manual play,
        // PlaybackController's own dispatch for auto-advance), so the chip is guarded by a lock -
        // StatusService itself already marshals the collection edit to the UI thread.
        private readonly object _preparingGate = new();
        private StatusHandle _preparingStatus;
        private int _preparingCount;

        // Separate chip from the per-track one above: the background pass runs alongside playback, so
        // both can legitimately be up at once.
        private readonly object _prewarmGate = new();
        private StatusHandle _prewarmStatus;

        // Raw Playlist for internal use - Playlists/SelectedPlaylistRow are the UI-facing wrappers
        // (rename state etc.), everything below just needs the underlying persisted model.
        private Playlist SelectedPlaylist => SelectedPlaylistRow?.Playlist;

        partial void OnSelectedPlaylistRowChanged(PlaylistRowViewModel value)
        {
            ClearQueue();
            NotifyAdvanceModeChanged();
            NotifyPlaybackModeChanged();
            if (value == null)
                return;

            foreach (var entry in value.Playlist.Entries)
                Queue.Add(new TrackCardViewModel(entry, this));
        }

        // Playback mode. Per playlist for the same reason as the advance trigger below, and pushed into
        // PlaybackController rather than only saved: it owns the play order, and a mode change has to
        // rebuild it around wherever the module currently is instead of waiting for the next start.
        private void SetPlaybackMode(PlaybackMode mode)
        {
            var playlist = SelectedPlaylist;
            if (playlist == null || playlist.Mode == mode)
            {
                NotifyPlaybackModeChanged();
                return;
            }

            playlist.Mode = mode;
            NotifyPlaybackModeChanged();
            _playback.RebuildOrder(playlist);
            SaveCurrentPlaylist();
        }

        private void NotifyPlaybackModeChanged() =>
            PlaybackModeOptionViewModel.SelectOnly(PlaybackModeOptions, SelectedPlaylist?.Mode ?? PlaybackMode.Sequential);

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
            OnPropertyChanged(nameof(DefaultCharacterName));
            _playback.DefaultCharacter = value;
            // Picking closes the panel: the header reads "Character: X" + "Change", i.e. as a dropdown,
            // and a dropdown that stays open after a choice reads as though the click missed.
            IsCharacterPickerExpanded = false;
        }

        [RelayCommand]
        private void ToggleCharacterPicker() => IsCharacterPickerExpanded = !IsCharacterPickerExpanded;

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
            // Captured before the await and acted on afterwards: the confirmation dialog gives the
            // user (and the overlay, which drives the same selection) a whole dialog's worth of time
            // to select something else or nothing at all. Re-reading SelectedPlaylistRow after the
            // dialog would either NRE on a cleared selection or delete a playlist the prompt never
            // named - so the answer applies to the row that was actually asked about.
            var removed = SelectedPlaylistRow;
            if (removed == null)
                return;

            if (!await ApplicationNotificationManager.Manager.AskForAction("Remove Playlist",
                    $"This will delete \"{removed.Name}\" and its saved tags/mods. Are you sure?"))
                return;

            // Gone already (deleted from the overlay while the dialog was up) - nothing left to do.
            var index = Playlists.IndexOf(removed);
            if (index < 0)
                return;

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

            // The play order is built from the entries, so it has to hear about this - a track added
            // mid-playback should be able to come up in the pass that is running.
            _playback.RebuildOrder(SelectedPlaylist);
            SaveCurrentPlaylist();
        }

        // EnsureConnected is checked first in every one of these - Prev used to fall through to a silent
        // no-op when nothing was playing (its "no current track" index fallback computed an
        // already-invalid prevIndex), so unlike Next it never even reached the connection check, which
        // read as "Prev is just broken" rather than "not connected". Checking connection unconditionally
        // up front makes every transport button behave the same.
        //
        // Which entry Next/Prev land on is PlaybackController's answer, not this class's: the same
        // question is asked by auto-advance and by the skip past a missing file, and three copies of the
        // arithmetic is how they drifted apart. It also cannot be done here at all any more - the order
        // depends on the playlist's mode, and under a shuffled one it is not index +- 1.
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

            var playlist = SelectedPlaylist;
            return Task.Run(() => _playback.PlayNext(playlist));
        }

        [RelayCommand]
        private Task Prev()
        {
            if (!EnsureConnected() || SelectedPlaylist == null)
                return Task.CompletedTask;

            var playlist = SelectedPlaylist;
            return Task.Run(() => _playback.PlayPrevious(playlist));
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
            card.Dispose();
            _playback.RebuildOrder(SelectedPlaylist);
            SaveCurrentPlaylist();
        }

        /// <summary>
        /// Empties the queue, releasing each card's decoded cover on the way out. Every card holds a
        /// Skia-backed <see cref="Avalonia.Media.Imaging.Bitmap"/>, whose memory the GC only gives
        /// back on a finalizer pass - a plain Queue.Clear() leaves a full playlist's worth of covers
        /// floating on every playlist switch.
        /// </summary>
        private void ClearQueue()
        {
            foreach (var card in Queue)
                card.Dispose();

            Queue.Clear();
        }

        /// <summary>
        /// Drops the card at <paramref name="from"/> into position <paramref name="to"/>, both being
        /// indices into the queue *after* the card is lifted out of it - the same convention
        /// ObservableCollection.Move uses. The view converts its own "insert before this card" into
        /// that (QuickPlayerView.ResolveDropIndex); doing it in one place is the whole reason this
        /// method takes a Move index rather than an insertion point.
        ///
        /// Move rather than remove-then-insert: it raises a single Move notification, so the ListBox
        /// keeps the container and the selection instead of rebuilding the row that was just dropped.
        /// </summary>
        public void MoveTrack(int from, int to)
        {
            var playlist = SelectedPlaylist;
            if (playlist == null || from == to)
                return;

            if (from < 0 || from >= Queue.Count || to < 0 || to >= Queue.Count)
                return;

            playlist.Entries.Move(from, to);
            Queue.Move(from, to);

            // The order is a list of playlist indices, so a reorder invalidates it outright - the same
            // index names a different entry now. This is also what re-derives the queue position, which
            // otherwise stays pointing at whatever slid into the slot the playing track left.
            _playback.RebuildOrder(playlist);
            SaveCurrentPlaylist();
        }

        public void SaveCurrentPlaylist()
        {
            var playlist = SelectedPlaylist;
            if (playlist == null)
                return;

            playlist.Save();
            PlaylistChanged?.Invoke(this, playlist);
        }

        // Interrupting a song means gotocharacterscreen - the game protocol has no pause - but whether
        // to send it at all depends on the module's phase, so that decision lives with the phase rather
        // than here (see PlaybackController.StopAndLeaveSong). The overlay's Stop routes through this
        // very command, so both surfaces get the check from one place.
        [RelayCommand]
        private void Stop() => _playback.StopAndLeaveSong();

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

        private void OnEntryUnavailable(PlaylistEntry entry)
        {
            Dispatcher.UIThread.Post(() => ApplicationNotificationManager.Manager.ShowError(
                "Track unavailable",
                $"\"{entry.SongTitle}\" is no longer at {entry.FilePath} - it was moved, renamed or deleted."));
        }

        // One chip for the whole background pass, updated in place, closed when it settles - the
        // per-entry reports are far too frequent to open a status entry each.
        private void OnPrewarmProgressed(PrewarmProgress progress)
        {
            StatusHandle finished = null;

            lock (_prewarmGate)
            {
                if (progress.IsComplete)
                {
                    finished = _prewarmStatus;
                    _prewarmStatus = null;
                }
                else
                {
                    var message = $"Preparing playlist: {progress.Settled}/{progress.Total}";
                    if (_prewarmStatus == null)
                        _prewarmStatus = StatusService.Manager.Begin(StatusToken.DiskProcess, "Quick Player", message);
                    else
                        _prewarmStatus.Update(message);
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

        /// <summary>
        /// Tears down everything this VM owns: the overlay bridge, the PlaybackController (which
        /// stops playback and drops its asconfig overrides), the queue's cover bitmaps and any status
        /// chips still up. Reached from MainWindowViewModel on application exit - the tab itself lives
        /// as long as the window does, so this is a shutdown path, not a hot one.
        /// </summary>
        public void Dispose()
        {
            if (_disposed)
                return;

            _disposed = true;

            // Bridge first: it listens to both this VM and the controller, and has no business
            // reacting to the unsubscribes and the stop that follow.
            _overlayBridge.Dispose();

            _playback.EntryPreparing -= OnEntryPreparing;
            _playback.EntryPrepared -= OnEntryPrepared;
            _playback.EntryStarted -= OnEntryStarted;
            _playback.EntryEnded -= OnEntryEnded;
            _playback.OperationFailed -= OnPlaybackFailed;
            _playback.EntryUnavailable -= OnEntryUnavailable;
            _playback.PrewarmProgressed -= OnPrewarmProgressed;
            _playback.Dispose();

            // Both are aliases into Queue, which owns the cards - dropped, not disposed, and dropped
            // before ClearQueue so neither outlives the objects it points at.
            SelectedTrackCard = null;
            CurrentTrackCard = null;
            ClearQueue();

            _nowPlayingStatus?.Dispose();
            _nowPlayingStatus = null;

            lock (_preparingGate)
            {
                _preparingStatus?.Dispose();
                _preparingStatus = null;
            }

            lock (_prewarmGate)
            {
                _prewarmStatus?.Dispose();
                _prewarmStatus = null;
            }
        }

        private bool _disposed;
    }
}
