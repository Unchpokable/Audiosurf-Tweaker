namespace QuickPlayerCore.Tests
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Threading;
    using NUnit.Framework;

    // Report semantics these tests encode, as verified against the running game:
    //   songcomplete       - track finished, player is on the leaderboard screen. playsong is accepted
    //                        from there, so auto-advance fires immediately.
    //   nowplaying*        - the game actually started the track we sent; a songcomplete only counts
    //                        against a confirmed start.
    //   oncharacterscreen  - only "the player navigated to the character screen". While Quick Player is
    //                        driving anything, that is the player taking over, and playback stops.
    // Auto-advance runs on a background thread, so anything asserting on a started track waits for it
    // (WaitForCommandCount) rather than reading the list straight after raising the report.
    [TestFixture]
    public class PlaybackControllerTests
    {
        [Test]
        public void SongComplete_StartsTheNextEntryWithoutWaitingForTheCharacterScreen()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
            Assert.IsTrue(controller.IsActive);
        }

        [Test]
        public void SongComplete_BeforeTheStartIsConfirmed_IsIgnored()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            reports.RaiseSongCompleted();

            Assert.IsFalse(game.WaitForCommandCount(2, 200), "an unconfirmed start was advanced past");
            Assert.AreSame(playlist.Entries[0], controller.CurrentEntry);
        }

        // The game sends one songcomplete per track, but a repeat must not cost the player an entry:
        // the second one arrives while the freshly started track is still unconfirmed.
        [Test]
        public void DuplicateSongComplete_DoesNotSkipAnEntry()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(3);

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();
            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");

            reports.RaiseSongCompleted();

            Assert.IsFalse(game.WaitForCommandCount(3, 200), "a duplicate songcomplete skipped an entry");
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
        }

        [Test]
        public void CharacterScreen_WhilePlaying_StopsPlaybackButKeepsQueuePosition()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(3);

            controller.Play(playlist, 1);
            reports.RaiseNowPlaying();
            reports.RaiseCharacterScreen();

            Assert.IsFalse(controller.IsActive);
            Assert.AreEqual(1, game.CommandCount);
            // The player bailed out - playback stops, but Next must continue from where they were
            // rather than restarting the playlist from the top.
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
        }

        [Test]
        public void CharacterScreen_BeforeTheStartIsConfirmed_AlsoStops()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            reports.RaiseCharacterScreen();

            Assert.IsFalse(controller.IsActive);
        }

        [Test]
        public void CharacterScreen_WhileIdle_DoesNothing()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            controller.Stop();
            var endedCount = 0;
            controller.EntryEnded += _ => endedCount++;

            // The game emits this on its way to the character screen at startup too, long before
            // anything is playing.
            reports.RaiseCharacterScreen();

            Assert.AreEqual(0, endedCount);
        }

        [Test]
        public void LastEntryCompletion_GoesIdle()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(1);

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsFalse(controller.IsActive);
            Assert.IsFalse(game.WaitForCommandCount(2, 200), "there was nothing left to advance to");
            Assert.AreSame(playlist.Entries[0], controller.CurrentEntry);
        }

        [Test]
        public void ConnectionLoss_StopsPlayback()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            game.SetValid(false);

            Assert.IsFalse(controller.IsActive);
            reports.RaiseSongCompleted();
            Assert.IsFalse(game.WaitForCommandCount(2, 200), "auto-advance survived the connection dropping");
        }

        [Test]
        public void Stop_EndsTrackButKeepsQueuePosition()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(3);
            PlaylistEntry ended = null;
            controller.EntryEnded += entry => ended = entry;

            controller.Play(playlist, 1);
            controller.Stop();

            Assert.IsFalse(controller.IsActive);
            Assert.AreSame(playlist.Entries[1], ended);
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
        }

        [Test]
        public void AutoAdvanceDisabled_StopsAfterTheCompletedEntry()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);
            playlist.AutoAdvance = false;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsFalse(controller.IsActive);
            Assert.IsFalse(game.WaitForCommandCount(2, 200), "auto-advance ran with AutoAdvance off");
        }

        // The status bar opens "Now playing" on EntryStarted and closes it on EntryEnded, so an
        // unmatched EntryStarted pins a stale chip in the bar forever. Bailing out to the character
        // screen at the moment a track starts used to do exactly that.
        [Test]
        public void StartedAndEnded_StayPairedAcrossEveryTransition()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(3);
            var depth = 0;
            var maxDepth = 0;
            controller.EntryStarted += _ => maxDepth = Math.Max(maxDepth, ++depth);
            controller.EntryEnded += _ => depth--;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();
            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");
            reports.RaiseNowPlaying();
            reports.RaiseCharacterScreen();

            Assert.AreEqual(1, maxDepth, "two tracks were announced as started at once");
            Assert.AreEqual(0, depth, "a started entry was never reported as ended");
        }

        // AdvanceTrigger.CharacterScreen - the player wants to read their score before the next track.
        [Test]
        public void WaitingForTheScoreScreen_AdvancesOnlyAfterTheCharacterScreen()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);
            playlist.AdvanceOn = AdvanceTrigger.CharacterScreen;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsFalse(game.WaitForCommandCount(2, 200), "the next track jumped the score screen");
            Assert.IsFalse(controller.IsPlaying);
            Assert.IsTrue(controller.IsActive, "the run is still going, it is just waiting");

            reports.RaiseCharacterScreen();

            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
        }

        // Same mode, but the player walked out mid-song instead of finishing it: no songcomplete came
        // first, so the very same report means "stop" rather than "go on".
        [Test]
        public void WaitingForTheScoreScreen_StillStopsOnABailOut()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);
            playlist.AdvanceOn = AdvanceTrigger.CharacterScreen;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseCharacterScreen();

            Assert.IsFalse(controller.IsActive);
            Assert.IsFalse(game.WaitForCommandCount(2, 200), "a bail-out advanced the playlist");
        }

        [Test]
        public void WaitingForTheScoreScreen_OnTheLastEntry_GoesIdle()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(1);
            playlist.AdvanceOn = AdvanceTrigger.CharacterScreen;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();
            reports.RaiseCharacterScreen();

            Assert.IsFalse(controller.IsActive);
            Assert.IsFalse(game.WaitForCommandCount(2, 200));
        }

        // Playing something by hand off the score screen has to cancel the queued advance, or the
        // player's own choice would be overwritten the moment they reach the character screen.
        [Test]
        public void WaitingForTheScoreScreen_ManualPlayCancelsTheQueuedAdvance()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(4);
            playlist.AdvanceOn = AdvanceTrigger.CharacterScreen;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();
            controller.Play(playlist, 3);

            Assert.AreEqual(2, game.CommandCount);
            Assert.AreSame(playlist.Entries[3], controller.CurrentEntry);
        }

        [Test]
        public void WaitingForTheScoreScreen_EndsTheTrackAtSongCompleteNotAtTheCharacterScreen()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);
            playlist.AdvanceOn = AdvanceTrigger.CharacterScreen;
            PlaylistEntry ended = null;
            controller.EntryEnded += entry => ended = entry;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            // The track really is over at songcomplete - its Quick Player overrides have to come back
            // off then, not linger while the player reads the leaderboard.
            Assert.AreSame(playlist.Entries[0], ended);
        }

        // A deleted or moved source file must cost one entry, not the whole run.
        [Test]
        public void AutoAdvance_SkipsPastAnEntryWhoseFileIsGone()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(3);
            PlaylistEntry unavailable = null;
            controller.EntryUnavailable += entry => unavailable = entry;
            File.Delete(playlist.Entries[1].FilePath);

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsTrue(game.WaitForCommandCount(2), "the playlist stopped at the missing entry");
            Assert.AreSame(playlist.Entries[1], unavailable);
            Assert.AreSame(playlist.Entries[2], controller.CurrentEntry);
        }

        [Test]
        public void AutoAdvance_StopsWhenEverythingLeftIsGone()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(3);
            File.Delete(playlist.Entries[1].FilePath);
            File.Delete(playlist.Entries[2].FilePath);

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsFalse(game.WaitForCommandCount(2, 500), "a missing entry was handed to the game anyway");
            Assert.IsFalse(controller.IsActive);
        }

        // Manual play is a specific choice, so a missing file reports and stops there - quietly playing
        // some other track instead would be worse than doing nothing.
        [Test]
        public void ManualPlay_OnAMissingFile_ReportsAndDoesNotSkip()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(3);
            PlaylistEntry unavailable = null;
            controller.EntryUnavailable += entry => unavailable = entry;
            File.Delete(playlist.Entries[1].FilePath);

            controller.Play(playlist, 1);

            Assert.AreSame(playlist.Entries[1], unavailable);
            Assert.AreEqual(0, game.CommandCount);
            Assert.IsFalse(controller.IsActive);
        }

        [Test]
        public void Playing_PrewarmsTheRestOfThePlaylistAndStopsWithIt()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            var prewarmer = new FakePrewarmer();
            using var controller = new PlaybackController(reports, game, prewarmer);
            var playlist = BuildPlaylist(3);

            controller.Play(playlist, 1);

            Assert.AreEqual(1, prewarmer.StartCount);
            Assert.AreSame(playlist, prewarmer.LastPlaylist);
            Assert.AreEqual(1, prewarmer.LastFromIndex, "prewarming has to start from what is playing");

            controller.Stop();

            Assert.AreEqual(1, prewarmer.StopCount);
        }

        // Reproduces the window that stranded the "Now playing" chip: the report lands after the phase
        // was claimed but before the start is announced. Driving it from the fake's Command callback
        // hits that exact ordering deterministically instead of racing two real threads for it.
        [Test]
        public void CharacterScreen_RacingTheStart_LeavesNoUnmatchedStart()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);
            var started = 0;
            var ended = 0;
            controller.EntryStarted += _ => started++;
            controller.EntryEnded += _ => ended++;
            game.OnCommand = () => reports.RaiseCharacterScreen();

            controller.Play(playlist, 0);

            Assert.IsFalse(controller.IsActive);
            Assert.AreEqual(started, ended, "a start was announced with no matching end");
        }

        // The QuickPlayer status chip is opened on EntryPreparing and closed on EntryPrepared, so an
        // unpaired Preparing would leave a chip stuck in the status bar forever.
        [Test]
        public void Preparing_IsAlwaysPairedAndPrecedesTheStart()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);
            var sequence = new List<string>();
            controller.EntryPreparing += _ => sequence.Add("preparing");
            controller.EntryPrepared += _ => sequence.Add("prepared");
            controller.EntryStarted += _ => sequence.Add("started");

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");
            // Both the manual play and the auto-advance report progress the same way.
            Assert.AreEqual(
                new[] { "preparing", "prepared", "started", "preparing", "prepared", "started" },
                sequence);
        }

        [Test]
        public void Preparing_IsStillPairedWhenTheEntryIndexIsOutOfRange()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(1);
            var preparing = 0;
            var prepared = 0;
            controller.EntryPreparing += _ => preparing++;
            controller.EntryPrepared += _ => prepared++;

            controller.Play(playlist, 5);

            Assert.AreEqual(0, game.CommandCount);
            Assert.IsFalse(controller.IsActive);
            Assert.AreEqual(preparing, prepared);
        }

        // Real files on disk: PlaybackController checks the source still exists before starting an
        // entry, so placeholder paths would make every one of these tests report "track unavailable".
        // Untagged, so TempFileTagger hands the original path straight back and never copies anything.
        private string _tracksDirectory;

        [SetUp]
        public void SetUp()
        {
            _tracksDirectory = Path.Combine(Path.GetTempPath(), "qp-tests-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(_tracksDirectory);
        }

        [TearDown]
        public void TearDown()
        {
            try
            {
                if (Directory.Exists(_tracksDirectory))
                    Directory.Delete(_tracksDirectory, recursive: true);
            }
            catch
            {
                // A leftover temp folder fails nothing.
            }
        }

        private Playlist BuildPlaylist(int count)
        {
            var playlist = new Playlist { Name = "Test" };
            for (var i = 0; i < count; i++)
            {
                var path = Path.Combine(_tracksDirectory, $"track-{i}.mp3");
                File.WriteAllBytes(path, Array.Empty<byte>());

                playlist.Entries.Add(new PlaylistEntry
                {
                    FilePath = path,
                    ArtistName = "Artist",
                    SongTitle = $"Track {i}"
                });
            }

            return playlist;
        }

        private sealed class FakePrewarmer : IPlaylistPrewarmer
        {
            public int StartCount { get; private set; }
            public int StopCount { get; private set; }
            public Playlist LastPlaylist { get; private set; }
            public int LastFromIndex { get; private set; } = -1;

            public void Start(Playlist playlist, int fromIndex)
            {
                StartCount++;
                LastPlaylist = playlist;
                LastFromIndex = fromIndex;
            }

            public void Stop() => StopCount++;

            public EntryReadiness GetReadiness(PlaylistEntry entry) => EntryReadiness.Unknown;

            public void Dispose()
            {
            }
        }

        private sealed class FakeReportSource : IPlaybackReportSource
        {
            public event Action<int> SongCompleted;
            public event Action OnCharacterScreen;
            public event Action NowPlaying;

            public void RaiseSongCompleted()
            {
                SongCompleted?.Invoke(123);
            }

            public void RaiseCharacterScreen()
            {
                OnCharacterScreen?.Invoke();
            }

            public void RaiseNowPlaying()
            {
                NowPlaying?.Invoke();
            }

            public void Dispose()
            {
            }
        }

        private sealed class FakeGameSession : IPlaybackGameSession
        {
            public event EventHandler StateChanged;

            private readonly object _gate = new();
            private readonly List<string> _commands = new();
            private bool _isValid = true;

            /// <summary>Runs inside Command(), i.e. at the point the game would have seen the playsong.</summary>
            public Action OnCommand { get; set; }

            public bool IsValid
            {
                get
                {
                    lock (_gate)
                        return _isValid;
                }
            }

            public int CommandCount
            {
                get
                {
                    lock (_gate)
                        return _commands.Count;
                }
            }

            public void Command(string command)
            {
                lock (_gate)
                {
                    _commands.Add(command);
                    Monitor.PulseAll(_gate);
                }

                OnCommand?.Invoke();
            }

            public void SetValid(bool value)
            {
                lock (_gate)
                    _isValid = value;
                StateChanged?.Invoke(this, EventArgs.Empty);
            }

            /// <summary>
            /// True once at least <paramref name="count"/> commands have been sent. Also used with a
            /// short timeout to assert the negative ("nothing else ever got sent"), which is why it
            /// returns a bool instead of asserting itself.
            /// </summary>
            public bool WaitForCommandCount(int count, int timeoutMs = 5000)
            {
                var deadline = Environment.TickCount64 + timeoutMs;
                lock (_gate)
                {
                    while (_commands.Count < count)
                    {
                        var remaining = (int)(deadline - Environment.TickCount64);
                        if (remaining <= 0 || !Monitor.Wait(_gate, remaining))
                            return _commands.Count >= count;
                    }

                    return true;
                }
            }
        }
    }
}
