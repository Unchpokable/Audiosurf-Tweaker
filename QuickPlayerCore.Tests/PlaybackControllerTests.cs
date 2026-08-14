namespace QuickPlayerCore.Tests
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Linq;
    using System.Threading;
    using AudiosurfInterface;
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

        // gotocharacterscreen sent while the game is not in a song leaves it drawing a white screen
        // instead of its UI (the game's own bug, recoverable only by starting another song). Stop
        // used to send it unconditionally, which the desktop hid by disabling its button when nothing
        // was playing - the overlay's Stop has no such guard and hit it immediately.
        [Test]
        public void StopAndLeaveSong_DuringAConfirmedTrack_AsksTheGameToLeaveIt()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            controller.StopAndLeaveSong();

            Assert.That(game.Commands, Does.Contain(GameProtocol.Command(GameProtocol.GoToCharacterScreen)));
        }

        [Test]
        public void StopAndLeaveSong_WithNothingPlaying_LeavesTheGameAlone()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());

            controller.StopAndLeaveSong();

            Assert.That(game.Commands, Is.Empty);
        }

        // Still preparing: the file copy has not finished, so no playsong has gone out and the player
        // is sitting on the character screen - the exact state that must not be told to go there.
        [Test]
        public void StopAndLeaveSong_BeforeTheStartIsConfirmed_LeavesTheGameAlone()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            controller.StopAndLeaveSong();

            Assert.That(game.Commands, Does.Not.Contain(GameProtocol.Command(GameProtocol.GoToCharacterScreen)));
        }

        // The player walked out of the song themselves, so the game is already where the command would
        // have sent it. This path goes through the internal Stop(), which must stay silent.
        [Test]
        public void WalkingOutMidSong_DoesNotSendGoToCharacterScreen()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseCharacterScreen();

            Assert.That(game.Commands, Does.Not.Contain(GameProtocol.Command(GameProtocol.GoToCharacterScreen)));
            Assert.IsFalse(controller.IsActive);
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

        // PlaybackMode.Single is the old AutoAdvance = false, which never had a UI to reach it.
        [Test]
        public void Single_StopsAfterTheCompletedEntry()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);
            playlist.Mode = PlaybackMode.Single;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsFalse(controller.IsActive);
            Assert.IsFalse(game.WaitForCommandCount(2, 200), "auto-advance ran in Single");
        }

        // Stopping on its own must not disable the transport - Single is "play this one", not "lock the
        // player onto it".
        [Test]
        public void Single_StillAdvancesOnAnExplicitNext()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);
            playlist.Mode = PlaybackMode.Single;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();
            controller.PlayNext(playlist);

            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
        }

        [Test]
        public void RepeatOne_StartsTheSameEntryAgain()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(3);
            playlist.Mode = PlaybackMode.RepeatOne;

            controller.Play(playlist, 1);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsTrue(game.WaitForCommandCount(2), "the track was never restarted");
            Assert.AreSame(playlist.Entries[1], controller.CurrentEntry);
        }

        [Test]
        public void RepeatAll_WrapsPastTheLastEntry()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(2);
            playlist.Mode = PlaybackMode.RepeatAll;

            controller.Play(playlist, 1);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsTrue(game.WaitForCommandCount(2), "the playlist did not wrap around");
            Assert.AreSame(playlist.Entries[0], controller.CurrentEntry);
        }

        // The bug this catches, found on the real game and reproduced here: Shuffle would play two tracks
        // out of eight and stop. The order was built when the mode was picked - nothing playing, nothing
        // to anchor to - and the pass was then left opening wherever the started track had landed in the
        // permutation, running only the tail after it. Driven through the controller rather than through
        // PlaybackOrder directly, because the order of calls is exactly what was wrong.
        [Test]
        public void Shuffle_PlaysEveryEntryOnceOverAWholePass()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer(), new Random(9));
            var playlist = BuildPlaylist(6);
            playlist.Mode = PlaybackMode.Shuffle;

            var started = new List<PlaylistEntry>();
            controller.EntryStarted += entry =>
            {
                lock (started)
                    started.Add(entry);
            };

            // The mode is picked before anything plays, which is when the app rebuilds the order.
            controller.RebuildOrder(playlist);
            controller.Play(playlist, 0);

            for (var i = 0; i < playlist.Entries.Count; i++)
            {
                reports.RaiseNowPlaying();
                reports.RaiseSongCompleted();
                if (!game.WaitForCommandCount(i + 2, 500))
                    break;
            }

            Assert.IsTrue(WaitForCount(started, playlist.Entries.Count), "the shuffled pass stopped early");
            Assert.That(started, Is.Unique);
            Assert.That(started, Is.EquivalentTo(playlist.Entries));
        }

        // EntryStarted is raised just after the playsong goes out, so a test that synchronised on the
        // command can be a hair ahead of the last one.
        private static bool WaitForCount(List<PlaylistEntry> started, int expected, int timeoutMs = 2000)
        {
            var deadline = Environment.TickCount64 + timeoutMs;
            while (Environment.TickCount64 < deadline)
            {
                lock (started)
                {
                    if (started.Count >= expected)
                        return true;
                }

                Thread.Sleep(10);
            }

            lock (started)
                return started.Count >= expected;
        }

        // The deferred advance of AdvanceTrigger.CharacterScreen is resolved at songcomplete but started
        // later, so a shuffled order has to survive the gap between the two.
        [Test]
        public void ShuffleLoop_AdvancesThroughTheScoreScreenWait()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer(), new Random(7));
            var playlist = BuildPlaylist(4);
            playlist.Mode = PlaybackMode.ShuffleLoop;
            playlist.AdvanceOn = AdvanceTrigger.CharacterScreen;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsFalse(game.WaitForCommandCount(2, 200), "the next track did not wait for the score screen");

            reports.RaiseCharacterScreen();

            Assert.IsTrue(game.WaitForCommandCount(2), "the deferred advance never ran");
            Assert.AreNotSame(playlist.Entries[0], controller.CurrentEntry);
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
            Assert.AreSame(playlist.Entries[2], prewarmer.LastOrder[0], "prewarming has to lead with what plays next");
            Assert.AreEqual(3, prewarmer.LastOrder.Count, "every entry has to be covered, or pruning deletes live copies");

            controller.Stop();

            Assert.AreEqual(1, prewarmer.StopCount);
        }

        // The whole point of handing the prewarmer an order instead of an index: under a shuffled mode
        // the track that plays next is not the next one in the list, and preparing the wrong file first
        // is the multi-megabyte copy the game ends up waiting on.
        [Test]
        public void Prewarming_FollowsTheShuffledOrderRatherThanThePlaylist()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            var prewarmer = new FakePrewarmer();
            using var controller = new PlaybackController(reports, game, prewarmer, new Random(3));
            var playlist = BuildPlaylist(6);
            playlist.Mode = PlaybackMode.Shuffle;

            controller.Play(playlist, 0);
            var expected = controller.GetUpcoming(1);

            Assert.AreEqual(1, expected.Count);
            Assert.AreSame(expected[0], prewarmer.LastOrder[0]);
            Assert.AreEqual(6, prewarmer.LastOrder.Count);
        }

        [Test]
        public void UpcomingAndRecent_TrackWhatActuallyPlayed()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(4);

            controller.Play(playlist, 1);

            Assert.That(controller.GetUpcoming(), Is.EqualTo(new[] { playlist.Entries[2], playlist.Entries[3] }));
            Assert.That(controller.GetRecent(), Is.EqualTo(new[] { playlist.Entries[0] }));
            Assert.AreEqual(PlaybackMode.Sequential, controller.CurrentMode);
        }

        // Editing the playlist while it plays - the queue position is an index, and every one of these
        // moves the entries out from under it. RebuildOrder re-derives the position from the entry it
        // was on; without that the index silently comes to name a different track.
        [Test]
        public void ReorderingThePlayingEntry_KeepsThePositionOnIt()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(5);

            controller.Play(playlist, 1);
            var playing = playlist.Entries[1];
            var expectedNext = playlist.Entries[4];

            // Dragged down the list: [0,2,3,1,4], so what follows it is no longer what followed it.
            playlist.Entries.Move(1, 3);
            controller.RebuildOrder(playlist);

            Assert.AreSame(playing, controller.CurrentEntry);

            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");
            Assert.AreSame(expectedNext, controller.CurrentEntry);
        }

        [Test]
        public void ReorderingAnEntryAboveTheCurrentOne_DoesNotSkipATrack()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(5);

            controller.Play(playlist, 3);
            var playing = playlist.Entries[3];
            var expectedNext = playlist.Entries[4];

            // Everything above the playing entry shifts up one: [1,2,3,4,0].
            playlist.Entries.Move(0, 4);
            controller.RebuildOrder(playlist);

            Assert.AreSame(playing, controller.CurrentEntry);

            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");
            Assert.AreSame(expectedNext, controller.CurrentEntry);
        }

        // Predates the drag-to-reorder work: removing an entry above the playing one left the index one
        // track too far down, and the next advance skipped a song.
        [Test]
        public void RemovingAnEntryAboveTheCurrentOne_DoesNotSkipATrack()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(4);

            controller.Play(playlist, 2);
            var playing = playlist.Entries[2];
            var expectedNext = playlist.Entries[3];

            playlist.Entries.RemoveAt(0);
            controller.RebuildOrder(playlist);

            Assert.AreSame(playing, controller.CurrentEntry);

            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            Assert.IsTrue(game.WaitForCommandCount(2), "the next entry was never started");
            Assert.AreSame(expectedNext, controller.CurrentEntry);
        }

        [Test]
        public void RemovingTheCurrentEntry_LeavesNoPositionInThePlaylist()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(3);

            controller.Play(playlist, 1);
            playlist.Entries.RemoveAt(1);
            controller.RebuildOrder(playlist);

            Assert.IsNull(controller.CurrentEntry, "the entry the module was on is not in the playlist any more");

            // No position means the order answers from its own start, exactly as it does for a playlist
            // that has never been played.
            Assert.IsTrue(controller.PlayNext(playlist));
            Assert.AreSame(playlist.Entries[0], controller.CurrentEntry);
        }

        // AdvanceTrigger.CharacterScreen decides the next entry at songcomplete and starts it later, and
        // the score screen is exactly where a player has time to reorder the queue. That decision was an
        // index into the order that no longer exists.
        [Test]
        public void ReorderingWhileWaitingForTheScoreScreen_AdvancesToTheNewNeighbour()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer());
            var playlist = BuildPlaylist(4);
            playlist.AdvanceOn = AdvanceTrigger.CharacterScreen;

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();
            reports.RaiseSongCompleted();

            var expectedNext = playlist.Entries[3];

            // The entry that just finished is dragged down the list: [1,2,0,3].
            playlist.Entries.Move(0, 2);
            controller.RebuildOrder(playlist);

            reports.RaiseCharacterScreen();

            Assert.IsTrue(game.WaitForCommandCount(2), "the deferred advance never ran");
            Assert.AreSame(expectedNext, controller.CurrentEntry);
        }

        // The order holds playlist indices, so a reorder invalidates it outright - the same index names a
        // different entry afterwards. Rebuilding restarts the pass around the playing track, which is the
        // accepted trade: what matters is that the pass is still a whole pass.
        [Test]
        public void Shuffle_SurvivesAReorderMidPass()
        {
            var reports = new FakeReportSource();
            var game = new FakeGameSession();
            using var controller = new PlaybackController(reports, game, new FakePrewarmer(), new Random(11));
            var playlist = BuildPlaylist(6);
            playlist.Mode = PlaybackMode.Shuffle;

            var started = new List<PlaylistEntry>();
            controller.EntryStarted += entry =>
            {
                lock (started)
                    started.Add(entry);
            };

            controller.Play(playlist, 0);
            reports.RaiseNowPlaying();

            playlist.Entries.Move(5, 0);
            controller.RebuildOrder(playlist);

            for (var i = 0; i < playlist.Entries.Count; i++)
            {
                reports.RaiseSongCompleted();
                if (!game.WaitForCommandCount(i + 2, 500))
                    break;

                reports.RaiseNowPlaying();
            }

            Assert.IsTrue(WaitForCount(started, playlist.Entries.Count), "the shuffled pass stopped early");
            Assert.That(started, Is.Unique);
            Assert.That(started, Is.EquivalentTo(playlist.Entries));
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
            public IReadOnlyList<PlaylistEntry> LastOrder { get; private set; } = Array.Empty<PlaylistEntry>();

            public void Start(Playlist playlist, IReadOnlyList<PlaylistEntry> order)
            {
                StartCount++;
                LastPlaylist = playlist;
                LastOrder = order;
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

            /// <summary>Everything sent so far, in order.</summary>
            public IReadOnlyList<string> Commands
            {
                get
                {
                    lock (_gate)
                        return _commands.ToList();
                }
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
