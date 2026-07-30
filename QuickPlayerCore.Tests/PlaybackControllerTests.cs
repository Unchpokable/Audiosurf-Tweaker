namespace QuickPlayerCore.Tests
{
    using System;
    using System.Collections.Generic;
    using System.Threading;
    using NUnit.Framework;

    // The game reports a finished track as songcomplete, then oncharacterscreen once the score screen
    // is dismissed; auto-advance is driven off that second report and runs on a background thread, so
    // anything asserting on a started track has to wait for it (WaitForCommandCount) rather than read
    // the list straight after raising the report.
    [TestFixture]
    public class PlaybackControllerTests
    {
        [Test]
        public void SongComplete_WaitsForCharacterScreenBeforeStartingNextEntry()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            reports.RaiseSongCompleted();

            Assert.AreEqual(1, game.CommandCount);
            Assert.IsFalse(controller.IsPlaying);

            reports.RaiseCharacterScreen();

            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
            Assert.IsTrue(controller.IsPlaying);
        }

        [Test]
        public void CharacterScreenWithoutSongComplete_StopsPlaybackButKeepsQueuePosition()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 1);
            reports.RaiseCharacterScreen();

            Assert.IsFalse(controller.IsPlaying);
            Assert.AreEqual(1, game.CommandCount);
            // The player bailed out - playback stops, but Next must continue from where they were
            // rather than restarting the playlist from the top.
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
        }

        // The regression this whole state machine exists for: hitting Next while the score screen is
        // still up used to arm "oncharacterscreen means the player bailed out", so the score screen's
        // own oncharacterscreen killed the track that had just been started by hand.
        [Test]
        public void ManualPlayAfterSongComplete_SurvivesTheTrailingCharacterScreen()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(4);

            controller.Play(playlist, 0);
            reports.RaiseSongCompleted();
            controller.Play(playlist, 3);

            reports.RaiseCharacterScreen();

            Assert.IsTrue(controller.IsPlaying);
            Assert.AreSame(playlist.Entries[3], controller.CurrentEntry);
            // Exactly the two explicit playsongs - the queued auto-advance to entry 1 was dropped by
            // the manual play, not run afterwards on top of it.
            Assert.AreEqual(2, game.CommandCount);
        }

        [Test]
        public void LastEntryCompletion_StopsAtCharacterScreen()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(1);

            controller.Play(playlist, 0);
            reports.RaiseSongCompleted();
            reports.RaiseCharacterScreen();

            Assert.IsFalse(controller.IsPlaying);
            Assert.AreEqual(1, game.CommandCount);
            Assert.AreSame(playlist.Entries[0], controller.CurrentEntry);
        }

        [Test]
        public void DuplicateSongComplete_DoesNotQueueMultipleEntries()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(3);

            controller.Play(playlist, 0);
            reports.RaiseSongCompleted();
            reports.RaiseSongCompleted();
            reports.RaiseCharacterScreen();

            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
            Assert.IsFalse(game.WaitForCommandCount(3, 200), "a duplicate songcomplete started a second track");
        }

        [Test]
        public void ConnectionLoss_StopsPlaybackAndClearsPendingAdvance()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            reports.RaiseSongCompleted();
            game.SetValid(false);
            reports.RaiseCharacterScreen();

            Assert.IsFalse(controller.IsPlaying);
            Assert.IsFalse(game.WaitForCommandCount(2, 200), "auto-advance survived the connection dropping");
        }

        [Test]
        public void Stop_EndsTrackButKeepsQueuePosition()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(3);
            PlaylistEntry ended = null;
            controller.EntryEnded += entry => ended = entry;

            controller.Play(playlist, 1);
            controller.Stop();

            Assert.IsFalse(controller.IsPlaying);
            Assert.AreSame(playlist.Entries[1], ended);
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
        }

        [Test]
        public void AutoAdvanceDisabled_StopsAfterTheCompletedEntry()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(2);
            playlist.AutoAdvance = false;

            controller.Play(playlist, 0);
            reports.RaiseSongCompleted();
            reports.RaiseCharacterScreen();

            Assert.IsFalse(controller.IsPlaying);
            Assert.IsFalse(game.WaitForCommandCount(2, 200), "auto-advance ran with AutoAdvance off");
        }

        // The QuickPlayer status chip is opened on EntryPreparing and closed on EntryPrepared, so an
        // unpaired Preparing would leave a chip stuck in the status bar forever.
        [Test]
        public void Preparing_IsAlwaysPairedAndPrecedesTheStart()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(2);
            var sequence = new List<string>();
            controller.EntryPreparing += _ => sequence.Add("preparing");
            controller.EntryPrepared += _ => sequence.Add("prepared");
            controller.EntryStarted += _ => sequence.Add("started");

            controller.Play(playlist, 0);
            reports.RaiseSongCompleted();
            reports.RaiseCharacterScreen();

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
            using var controller = new PlaybackController(reports, game);
            var playlist = BuildPlaylist(1);
            var preparing = 0;
            var prepared = 0;
            controller.EntryPreparing += _ => preparing++;
            controller.EntryPrepared += _ => prepared++;

            controller.Play(playlist, 5);

            Assert.AreEqual(0, game.CommandCount);
            Assert.AreEqual(preparing, prepared);
        }

        private static Playlist BuildPlaylist(int count)
        {
            var playlist = new Playlist { Name = "Test" };
            for (var i = 0; i < count; i++)
            {
                playlist.Entries.Add(new PlaylistEntry
                {
                    FilePath = $"track-{i}.mp3",
                    ArtistName = "Artist",
                    SongTitle = $"Track {i}"
                });
            }

            return playlist;
        }

        private sealed class FakeReportSource : IPlaybackReportSource
        {
            public event Action<int> SongCompleted;
            public event Action OnCharacterScreen;

            public void RaiseSongCompleted()
            {
                SongCompleted?.Invoke(123);
            }

            public void RaiseCharacterScreen()
            {
                OnCharacterScreen?.Invoke();
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
