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
        event Action NowPlaying;
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
            _listener.NowPlaying += (_, _, _) => NowPlaying?.Invoke();
        }

        public event Action<int> SongCompleted;
        public event Action OnCharacterScreen;
        public event Action NowPlaying;

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
    /// the phase below is what actually goes idle.
    ///
    /// What the reports mean, as verified against the running game:
    /// - songcomplete - the track finished and the player is on the leaderboard screen. The game
    ///   accepts playsong from there (and mid-song), so nothing forces a wait before the next track.
    /// - nowplaying* - the game really did start the song we asked for. This is the only confirmation
    ///   there is, so a songcomplete only counts once the start it belongs to has been confirmed;
    ///   otherwise a repeated report could advance the queue twice for one track.
    /// - oncharacterscreen - literally just "the player navigated to the character screen", nothing
    ///   more. It carries no meaning on its own; what it means depends on what came before it.
    ///
    /// Which is what the two AdvanceTrigger modes are built out of. songcomplete then
    /// oncharacterscreen is "the track ended and the player finished reading their score"; an
    /// oncharacterscreen with no songcomplete in front of it is "the player walked out mid-song" and
    /// ends the run in either mode. SongComplete mode simply doesn't wait for the second half.
    /// </summary>
    public sealed class PlaybackController : IDisposable
    {
        private enum PlaybackPhase
        {
            Idle,

            /// <summary>playsong is being prepared or has been sent; the game hasn't confirmed it yet.</summary>
            Starting,

            /// <summary>The game reported nowplaying* for the track this controller started.</summary>
            Playing,

            /// <summary>
            /// songcomplete arrived and the playlist wants the player to see their score first
            /// (AdvanceTrigger.CharacterScreen) - the track is over, the next one waits for the
            /// oncharacterscreen that says the player is done looking.
            /// </summary>
            Completed
        }

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
            _reportSource.NowPlaying += OnNowPlaying;
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

        /// <summary>
        /// Strictly alternating: an EntryStarted is always followed by exactly one EntryEnded before
        /// the next EntryStarted, and a start that loses its race against an end is never announced at
        /// all (see NotifyStarted). A UI status opened on Started and closed on Ended therefore cannot
        /// be orphaned - which it was, when bailing out to the character screen at the exact moment a
        /// track began left "Now playing" pinned in the status bar forever.
        /// </summary>
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

        /// <summary>Quick Player is driving a track - either starting one or playing one.</summary>
        public bool IsActive
        {
            get
            {
                lock (_gate)
                    return _phase != PlaybackPhase.Idle;
            }
        }

        /// <summary>The game confirmed (via nowplaying*) that it started the track this controller sent.</summary>
        public bool IsPlaying
        {
            get
            {
                lock (_gate)
                    return _phase == PlaybackPhase.Playing;
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
        private PlaybackPhase _phase = PlaybackPhase.Idle;

        // Only used while Completed: where to go once the player closes the score screen. Cleared by
        // anything that takes over the queue (an explicit Play, a Stop) so a deferred advance can never
        // fire on top of a decision the user made after it was queued.
        private int? _pendingAdvanceIndex;

        // Bumped by every Play/Stop. A start that was superseded while it was preparing its file
        // (TempFileTagger can copy and retag, which takes real time) sees a stale token and backs out
        // instead of sending a playsong for a track the user has already moved on from.
        private long _startToken;

        // Serializes the EntryStarted/EntryEnded pair against each other only - deliberately separate
        // from _gate, which cannot be held across an event invocation. Taken before _gate on the one
        // path that needs both (NotifyStarted); nothing takes them in the other order.
        private readonly object _notifyGate = new();
        private PlaylistEntry _notifiedEntry;

        public void Play(Playlist playlist, int index)
        {
            StartEntry(playlist, index);
        }

        /// <summary>
        /// Ends the current track and cancels any start still being prepared. Keeps the queue position -
        /// the module goes idle, it doesn't forget where it was.
        /// </summary>
        public void Stop()
        {
            lock (_gate)
            {
                _startToken++;
                _pendingAdvanceIndex = null;
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

                // Claimed before the slow preparation below, so an oncharacterscreen arriving while the
                // file is still being copied is recognised as the player taking over and cancels it.
                _phase = entry != null ? PlaybackPhase.Starting : PlaybackPhase.Idle;
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
                    _activeOverrides.AddRange(handles);
            }

            if (superseded)
            {
                for (var i = handles.Count - 1; i >= 0; i--)
                    handles[i].Dispose();
                return;
            }

            _gameSession.Command(GameProtocol.PlaySong(entry.Character ?? DefaultCharacter, resolvedPath));
            NotifyStarted(entry, token);
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

        private void OnNowPlaying()
        {
            lock (_gate)
            {
                if (_phase == PlaybackPhase.Starting)
                    _phase = PlaybackPhase.Playing;
            }
        }

        private void OnSongCompleted(int score)
        {
            Playlist playlist;
            int? next;
            bool deferred;

            lock (_gate)
            {
                // Only a confirmed start can complete. Without this a second songcomplete for the same
                // track would advance the queue again and silently skip an entry.
                if (_phase != PlaybackPhase.Playing || _playlist == null)
                    return;

                playlist = _playlist;
                next = playlist.AutoAdvance && _currentIndex + 1 < playlist.Entries.Count
                    ? _currentIndex + 1
                    : null;

                deferred = playlist.AdvanceOn == AdvanceTrigger.CharacterScreen && next.HasValue;
                _pendingAdvanceIndex = deferred ? next : null;
            }

            // The phase moves straight from Playing to Completed under the same call that ends the
            // track - going through Idle first would leave a window where the oncharacterscreen we are
            // waiting for arrives, sees an idle controller and is discarded as "nothing to do".
            EndCurrent(deferred ? PlaybackPhase.Completed : PlaybackPhase.Idle);

            if (!deferred && next.HasValue)
                StartInBackground(playlist, next.Value);
        }

        private void OnCharacterScreenReached()
        {
            Playlist playlist = null;
            int? next = null;

            lock (_gate)
            {
                switch (_phase)
                {
                    // Nothing of ours is running - the game emits this on its way to the character
                    // screen at startup too.
                    case PlaybackPhase.Idle:
                        return;

                    // The track finished and the player has now closed their score: the awaited half of
                    // songcomplete + oncharacterscreen.
                    case PlaybackPhase.Completed:
                        playlist = _playlist;
                        next = _pendingAdvanceIndex;
                        _pendingAdvanceIndex = null;
                        _phase = PlaybackPhase.Idle;
                        break;
                }
            }

            if (next.HasValue && playlist != null)
            {
                StartInBackground(playlist, next.Value);
                return;
            }

            // Starting or Playing: no songcomplete came first, so the player walked out of the song by
            // hand. That, and reaching Completed with nothing left to advance to, both end the run.
            Stop();
        }

        private void OnGameStateChanged(object sender, EventArgs e)
        {
            if (!_gameSession.IsValid)
                Stop();
        }

        // Reports are dispatched onto whatever SynchronizationContext AudiosurfHandle captured (in the
        // app, the UI thread), and StartEntry's file copy/retag would block it - the manual path goes
        // through Task.Run in the ViewModel for the very same reason.
        private void StartInBackground(Playlist playlist, int index)
        {
            _ = Task.Run(() =>
            {
                try
                {
                    StartEntry(playlist, index);
                }
                catch (Exception ex)
                {
                    OperationFailed?.Invoke("Failed to start the next track in the playlist", ex);
                }
            });
        }

        private void EndCurrent(PlaybackPhase into = PlaybackPhase.Idle)
        {
            List<IDisposable> overrides;

            lock (_gate)
            {
                if (_phase == PlaybackPhase.Idle)
                    return;

                _phase = into;
                overrides = new List<IDisposable>(_activeOverrides);
                _activeOverrides.Clear();
            }

            for (var i = overrides.Count - 1; i >= 0; i--)
                overrides[i].Dispose();

            NotifyEnded();
        }

        // The start is announced only if it is still the current one and hasn't already been ended -
        // both checked while holding _notifyGate, so an EndCurrent racing this either sees the entry
        // announced (and ends it) or wins and makes this a no-op. Announcing unconditionally is what
        // used to strand the "Now playing" status.
        private void NotifyStarted(PlaylistEntry entry, long token)
        {
            lock (_notifyGate)
            {
                lock (_gate)
                {
                    if (_startToken != token || _phase == PlaybackPhase.Idle)
                        return;
                }

                _notifiedEntry = entry;
                EntryStarted?.Invoke(entry);
            }
        }

        // Ends the entry that was actually announced, not whatever the queue position points at now -
        // a Play() that supersedes a track moves the position before the old track's end is reported.
        private void NotifyEnded()
        {
            lock (_notifyGate)
            {
                var announced = _notifiedEntry;
                if (announced == null)
                    return;

                _notifiedEntry = null;
                EntryEnded?.Invoke(announced);
            }
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
            _reportSource.NowPlaying -= OnNowPlaying;
            _reportSource.Dispose();
            _gameSession.StateChanged -= OnGameStateChanged;
        }
    }
}
