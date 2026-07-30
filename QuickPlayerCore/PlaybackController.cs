using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using AudiosurfInterface;
using QuickPlayerCore.Audiosurf;

namespace QuickPlayerCore
{
    internal interface IPlaybackReportSource : IDisposable
    {
        event Action<int> SongCompleted;
        event Action OnCharacterScreen;
    }

    internal interface IPlaybackGameSession
    {
        event EventHandler StateChanged;
        bool IsValid { get; }
        void Command(string command);
    }

    internal sealed class PlaybackReportSource : IPlaybackReportSource
    {
        public PlaybackReportSource()
        {
            _listener = new GameReportListener();
            _listener.SongCompleted += score => SongCompleted?.Invoke(score);
            _listener.OnCharacterScreen += () => OnCharacterScreen?.Invoke();
        }

        public event Action<int> SongCompleted;
        public event Action OnCharacterScreen;

        private readonly GameReportListener _listener;

        public void Dispose()
        {
            _listener.Dispose();
        }
    }

    internal sealed class PlaybackGameSession : IPlaybackGameSession
    {
        public event EventHandler StateChanged
        {
            add => AudiosurfHandle.Instance.StateChanged += value;
            remove => AudiosurfHandle.Instance.StateChanged -= value;
        }

        public bool IsValid => AudiosurfHandle.Instance.IsValid;

        public void Command(string command)
        {
            AudiosurfHandle.Instance.Command(command);
        }
    }

    /// <summary>
    /// Drives playback of a Playlist: resolves the file to hand the game (original or a TempFileTagger
    /// copy), pushes the entry's config overrides (explicit ConfigOverrides plus any AsConfigBinding
    /// tags) through GameConfigState, and sends the playsong command.
    ///
    /// Two pieces of state that used to be one, and must not be conflated again: the *position*
    /// (Playlist + index, i.e. "where the module is in the queue") survives a track ending, so Next
    /// after a manual bail-out continues from where the user was instead of restarting the playlist;
    /// IsPlaying ("a playsong was sent and the track hasn't ended yet") is what actually goes false.
    ///
    /// End of a track is reported by the game as songcomplete, followed - once the score screen is
    /// dismissed - by oncharacterscreen. Auto-advance waits for that second report because the game
    /// only reliably accepts a playsong from the character screen. An oncharacterscreen the module was
    /// *not* already expecting means the player bailed out of the song themselves, and stops playback.
    /// </summary>
    public sealed class PlaybackController : IDisposable
    {
        public PlaybackController()
            : this(new PlaybackReportSource(), new PlaybackGameSession())
        {
        }

        internal PlaybackController(IPlaybackReportSource reportSource, IPlaybackGameSession gameSession)
        {
            _reportSource = reportSource;
            _gameSession = gameSession;
            _reportSource.SongCompleted += OnSongCompleted;
            _reportSource.OnCharacterScreen += OnCharacterScreenReached;
            _gameSession.StateChanged += OnGameStateChanged;
        }

        /// <summary>
        /// Bracket around the part of a start that can take real time - TempFileTagger may copy the
        /// file and rewrite its tags. EntryPrepared always follows EntryPreparing (it is raised from a
        /// finally), so a UI status bound to this pair can never be left hanging, including when the
        /// start is superseded or throws. Auto-advance goes through the same pair as a manual play, so
        /// the user sees the module working in both cases and not only when they clicked something.
        /// </summary>
        public event Action<PlaylistEntry> EntryPreparing;
        public event Action<PlaylistEntry> EntryPrepared;

        public event Action<PlaylistEntry> EntryStarted;
        public event Action<PlaylistEntry> EntryEnded;

        /// <summary>
        /// An auto-advance start failed. Auto-advance runs on a background thread (see StartEntry), so
        /// unlike the caller-driven Play() there is nobody to propagate the exception to - reported as
        /// an event rather than through a Logger dependency, same convention as TempFileTagger/Playlist.
        /// </summary>
        public event Action<string, Exception> OperationFailed;

        /// <summary>
        /// Where the module currently is in the queue - NOT "what is audibly playing" (see IsPlaying).
        /// Deliberately survives a track ending so the transport's Next/Prev keep their place.
        /// </summary>
        public PlaylistEntry CurrentEntry
        {
            get
            {
                lock (_gate)
                    return CurrentEntryLocked();
            }
        }

        /// <summary>A playsong has been sent for CurrentEntry and the game hasn't reported it over yet.</summary>
        public bool IsPlaying
        {
            get
            {
                lock (_gate)
                    return _trackInFlight;
            }
        }

        /// <summary>Character to launch with when the entry itself has no override (bound to the transport bar's character grid).</summary>
        public GameCharacter DefaultCharacter { get; set; } = GameCharacter.Mono;

        private readonly IPlaybackReportSource _reportSource;
        private readonly IPlaybackGameSession _gameSession;
        private readonly List<IDisposable> _activeOverrides = new();

        // Guards every field below. Never held across a call into GameConfigState/IPlaybackGameSession:
        // AudiosurfHandle.HandleReport holds its own lock while invoking the report handlers that call
        // into this class, so taking this lock around an outbound Command() would invert the lock order.
        private readonly object _gate = new();

        private Playlist _playlist;
        private int _currentIndex = -1;
        private bool _trackInFlight;

        // A songcomplete was seen and its trailing oncharacterscreen hasn't arrived yet. Survives an
        // explicit Play() on purpose: hitting Next while the score screen is still up used to clear
        // this, so the score screen's own oncharacterscreen was then misread as "the player bailed out"
        // and killed the freshly started track.
        private bool _expectCharacterScreen;
        private int? _pendingAdvanceIndex;

        // Bumped by every Play/Stop. A start that was superseded while it was preparing its file
        // (TempFileTagger can copy and retag, which takes real time) sees a stale token and backs out
        // instead of sending a playsong for a track the user has already moved on from.
        private long _startToken;

        public void Play(Playlist playlist, int index)
        {
            StartEntry(playlist, index);
        }

        /// <summary>
        /// Ends the current track and cancels any queued auto-advance. Keeps the queue position - the
        /// module goes idle, it doesn't forget where it was.
        /// </summary>
        public void Stop()
        {
            lock (_gate)
            {
                _startToken++;
                _pendingAdvanceIndex = null;
                _expectCharacterScreen = false;
            }

            EndCurrent();
        }

        private void StartEntry(Playlist playlist, int index)
        {
            EndCurrent();

            long token;
            PlaylistEntry entry;

            lock (_gate)
            {
                token = ++_startToken;
                _pendingAdvanceIndex = null;
                _playlist = playlist;
                _currentIndex = index;
                entry = CurrentEntryLocked();
            }

            if (entry == null)
                return;

            // Cold path, deliberately outside the lock: ResolvePlaybackPath can copy the file and
            // rewrite its tags.
            string resolvedPath;
            Dictionary<string, bool> overrides;
            EntryPreparing?.Invoke(entry);
            try
            {
                resolvedPath = TempFileTagger.ResolvePlaybackPath(playlist.Id, entry);
                overrides = CollectOverrides(entry);
            }
            finally
            {
                EntryPrepared?.Invoke(entry);
            }

            var handles = new List<IDisposable>(overrides.Count);
            foreach (var (key, value) in overrides)
                handles.Add(GameConfigState.Manager.PushOverride(key, value, GameConfigOverrideSource.QuickPlayer));

            bool superseded;
            lock (_gate)
            {
                superseded = _startToken != token;
                if (!superseded)
                {
                    _activeOverrides.AddRange(handles);
                    _trackInFlight = true;
                }
            }

            if (superseded)
            {
                for (var i = handles.Count - 1; i >= 0; i--)
                    handles[i].Dispose();
                return;
            }

            _gameSession.Command(GameProtocol.PlaySong(entry.Character ?? DefaultCharacter, resolvedPath));
            EntryStarted?.Invoke(entry);
        }

        // Both sources here are the same mechanism - a temporary asconfig value layered over the user's
        // global one - so there is no "tag beats mod" precedence to decide: SidewinderCamera and
        // BankingCamera are the only tags with an AsConfigBinding, and TagOptionViewModel deliberately
        // hides those from the Tags list because ModOptionViewModel already exposes them as Mods. The
        // UI therefore cannot produce an entry that sets one key from both sides. The dictionary is a
        // guard for hand-edited or imported playlist JSON, where pushing both would stack two overrides
        // on one key and let their restore order decide the outcome.
        private static Dictionary<string, bool> CollectOverrides(PlaylistEntry entry)
        {
            var overrides = new Dictionary<string, bool>(entry.ConfigOverrides);
            foreach (var tag in entry.Tags)
            {
                if (SongTagCatalog.Get(tag.Token).AsConfigBinding is { } binding)
                    overrides[binding.ConfigKey] = binding.Value;
            }

            return overrides;
        }

        private void OnSongCompleted(int score)
        {
            lock (_gate)
            {
                if (!_trackInFlight || _playlist == null)
                    return;

                // The game follows this with oncharacterscreen once the score screen is dismissed; that
                // report is the expected tail of this completion, not the player walking out.
                _expectCharacterScreen = true;
                _pendingAdvanceIndex = _playlist.AutoAdvance && _currentIndex + 1 < _playlist.Entries.Count
                    ? _currentIndex + 1
                    : null;
            }

            EndCurrent();
        }

        private void OnCharacterScreenReached()
        {
            Playlist playlist;
            int? next;
            bool expected;
            bool inFlight;

            lock (_gate)
            {
                expected = _expectCharacterScreen;
                _expectCharacterScreen = false;
                next = _pendingAdvanceIndex;
                _pendingAdvanceIndex = null;
                playlist = _playlist;
                inFlight = _trackInFlight;
            }

            if (next.HasValue && playlist != null)
            {
                // Off the report thread: reports are dispatched onto whatever SynchronizationContext
                // AudiosurfHandle captured (in the app, the UI thread), and StartEntry's file copy/retag
                // would block it - the manual path goes through Task.Run in the ViewModel for the very
                // same reason.
                var target = next.Value;
                _ = Task.Run(() =>
                {
                    try
                    {
                        StartEntry(playlist, target);
                    }
                    catch (Exception ex)
                    {
                        OperationFailed?.Invoke("Failed to start the next track in the playlist", ex);
                    }
                });
                return;
            }

            // Already accounted for by a songcomplete. Nothing left to advance to (end of playlist), or
            // an explicit Play() has since taken over - either way this is not a bail-out.
            if (expected)
                return;

            if (inFlight)
                EndCurrent();
        }

        private void OnGameStateChanged(object sender, EventArgs e)
        {
            if (!_gameSession.IsValid)
                Stop();
        }

        private void EndCurrent()
        {
            PlaylistEntry entry;
            List<IDisposable> overrides;

            lock (_gate)
            {
                if (!_trackInFlight)
                    return;

                _trackInFlight = false;
                entry = CurrentEntryLocked();
                overrides = new List<IDisposable>(_activeOverrides);
                _activeOverrides.Clear();
            }

            for (var i = overrides.Count - 1; i >= 0; i--)
                overrides[i].Dispose();

            if (entry != null)
                EntryEnded?.Invoke(entry);
        }

        private PlaylistEntry CurrentEntryLocked() =>
            _playlist != null && _currentIndex >= 0 && _currentIndex < _playlist.Entries.Count
                ? _playlist.Entries[_currentIndex]
                : null;

        public void Dispose()
        {
            Stop();
            _reportSource.SongCompleted -= OnSongCompleted;
            _reportSource.OnCharacterScreen -= OnCharacterScreenReached;
            _reportSource.Dispose();
            _gameSession.StateChanged -= OnGameStateChanged;
        }
    }
}
