using System;
using System.Collections.Generic;
using AudiosurfInterface;
using QuickPlayerCore.Audiosurf;

namespace QuickPlayerCore
{
    /// <summary>
    /// Drives sequential playback of a Playlist: resolves the file to hand the game (original or a
    /// TempFileTagger copy), pushes the entry's config overrides (explicit ConfigOverrides plus any
    /// AsConfigBinding tags) through GameConfigState, and sends the playsong command. A track can end
    /// three different ways - normal completion, the player bailing out to the character screen early,
    /// or the bridge/game connection dropping mid-song - all three converge on the same EndCurrent(),
    /// guarded by a disposed-style flag so overrides/EntryEnded never fire twice for one track.
    /// </summary>
    public sealed class PlaybackController : IDisposable
    {
        public PlaybackController()
        {
            _reportListener = new GameReportListener();
            _reportListener.SongCompleted += OnSongCompleted;
            _reportListener.OnCharacterScreen += OnCharacterScreenReached;
            AudiosurfHandle.Instance.StateChanged += OnGameStateChanged;
        }

        public event Action<PlaylistEntry> EntryStarted;
        public event Action<PlaylistEntry> EntryEnded;

        public PlaylistEntry CurrentEntry =>
            _playlist != null && _currentIndex >= 0 && _currentIndex < _playlist.Entries.Count
                ? _playlist.Entries[_currentIndex]
                : null;

        /// <summary>Character to launch with when the entry itself has no override (bound to the transport bar's character grid).</summary>
        public GameCharacter DefaultCharacter { get; set; } = GameCharacter.Mono;

        private readonly GameReportListener _reportListener;
        private readonly List<IDisposable> _activeOverrides = new();
        private Playlist _playlist;
        private int _currentIndex = -1;
        private bool _currentEnded = true;

        public void Play(Playlist playlist, int index)
        {
            EndCurrent();

            _playlist = playlist;
            _currentIndex = index;
            _currentEnded = false;

            var entry = CurrentEntry;
            if (entry == null)
                return;

            var resolvedPath = TempFileTagger.ResolvePlaybackPath(playlist.Id, entry);

            foreach (var (key, value) in entry.ConfigOverrides)
                _activeOverrides.Add(GameConfigState.Manager.PushOverride(key, value));

            foreach (var tag in entry.Tags)
            {
                if (SongTagCatalog.Get(tag.Token).AsConfigBinding is { } binding)
                    _activeOverrides.Add(GameConfigState.Manager.PushOverride(binding.ConfigKey, binding.Value));
            }

            AudiosurfHandle.Instance.Command(GameProtocol.PlaySong(entry.Character ?? DefaultCharacter, resolvedPath));
            EntryStarted?.Invoke(entry);
        }

        public void Stop()
        {
            EndCurrent();
            _playlist = null;
            _currentIndex = -1;
        }

        private void OnSongCompleted(int score)
        {
            var playlist = _playlist;
            var index = _currentIndex;
            EndCurrent();

            if (playlist != null && playlist.AutoAdvance && index + 1 < playlist.Entries.Count)
                Play(playlist, index + 1);
        }

        private void OnCharacterScreenReached() => EndCurrent();

        private void OnGameStateChanged(object sender, EventArgs e)
        {
            if (!AudiosurfHandle.Instance.IsValid)
                EndCurrent();
        }

        private void EndCurrent()
        {
            if (_currentEnded)
                return;
            _currentEnded = true;

            var entry = CurrentEntry;

            for (var i = _activeOverrides.Count - 1; i >= 0; i--)
                _activeOverrides[i].Dispose();
            _activeOverrides.Clear();

            if (entry != null)
                EntryEnded?.Invoke(entry);
        }

        public void Dispose()
        {
            EndCurrent();
            _reportListener.SongCompleted -= OnSongCompleted;
            _reportListener.OnCharacterScreen -= OnCharacterScreenReached;
            _reportListener.Dispose();
            AudiosurfHandle.Instance.StateChanged -= OnGameStateChanged;
        }
    }
}
