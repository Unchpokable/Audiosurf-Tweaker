namespace QuickPlayerCore.Tests
{
    using System;
    using System.Collections.Generic;
    using NUnit.Framework;

    // No files on disk anywhere in here: PlaybackOrder answers "which entry", never "is it playable" -
    // that check belongs to PlaybackController.StartEntry and is covered by its own tests.
    // Shuffled cases run on a seeded Random so the permutations are reproducible, and assert the
    // property the mode promises rather than one particular shuffle.
    [TestFixture]
    public class PlaybackOrderTests
    {
        [Test]
        public void Sequential_WalksTheListAndStopsAtBothEnds()
        {
            var playlist = BuildPlaylist(3);
            var order = new PlaybackOrder();

            Assert.AreEqual(1, order.Next(playlist, 0, isAutoAdvance: true));
            Assert.AreEqual(0, order.Previous(playlist, 1));
            Assert.IsNull(order.Next(playlist, 2, isAutoAdvance: true));
            Assert.IsNull(order.Previous(playlist, 0));
        }

        [Test]
        public void Single_EndsTheRunButNotTheTransport()
        {
            var playlist = BuildPlaylist(3, PlaybackMode.Single);
            var order = new PlaybackOrder();

            Assert.IsNull(order.Next(playlist, 0, isAutoAdvance: true), "Single advanced by itself");
            Assert.AreEqual(1, order.Next(playlist, 0, isAutoAdvance: false));
            Assert.AreEqual(0, order.Previous(playlist, 1));
        }

        // Repeating is a property of a track finishing, not of the Next button - otherwise the mode traps
        // the player on one song with no way out but stopping.
        [Test]
        public void RepeatOne_RepeatsOnCompletionOnlyAndSkipsLikeSequential()
        {
            var playlist = BuildPlaylist(3, PlaybackMode.RepeatOne);
            var order = new PlaybackOrder();

            Assert.AreEqual(1, order.Next(playlist, 1, isAutoAdvance: true));
            Assert.AreEqual(2, order.Next(playlist, 1, isAutoAdvance: false));
            Assert.AreEqual(0, order.Previous(playlist, 1));
            Assert.IsNull(order.Next(playlist, 2, isAutoAdvance: false), "RepeatOne wrapped on a manual skip");
        }

        [Test]
        public void RepeatAll_WrapsBothWays()
        {
            var playlist = BuildPlaylist(3, PlaybackMode.RepeatAll);
            var order = new PlaybackOrder();

            Assert.AreEqual(0, order.Next(playlist, 2, isAutoAdvance: true));
            Assert.AreEqual(2, order.Previous(playlist, 0));
        }

        [Test]
        public void ALoopingModeOnASingleEntryPlaylistReplaysIt()
        {
            var repeatAll = BuildPlaylist(1, PlaybackMode.RepeatAll);
            Assert.AreEqual(0, new PlaybackOrder().Next(repeatAll, 0, isAutoAdvance: true));

            var shuffleLoop = BuildPlaylist(1, PlaybackMode.ShuffleLoop);
            Assert.AreEqual(0, new PlaybackOrder(new Random(1)).Next(shuffleLoop, 0, isAutoAdvance: true));
        }

        [Test]
        public void Shuffle_CoversEveryEntryExactlyOnceThenStops()
        {
            var playlist = BuildPlaylist(6, PlaybackMode.Shuffle);
            var order = new PlaybackOrder(new Random(11));
            order.Rebuild(playlist, 0);

            var played = Walk(order, playlist, 0, 20);

            Assert.That(played, Is.EquivalentTo(new[] { 0, 1, 2, 3, 4, 5 }), "a pass has to hit every entry once");
        }

        // The regression this class was rewritten for. The app builds the order when the mode is picked -
        // with nothing playing yet - and only afterwards does the user press Play on a track. The pass
        // used to be left opening wherever that track had landed in the permutation and to run only the
        // tail after it, so Shuffle would play two songs out of eight and stop. Every seed has to cover
        // the whole playlist, so this runs a spread of them rather than one lucky one.
        [Test]
        public void Shuffle_StartedAfterTheOrderWasBuilt_StillCoversEverything()
        {
            for (var seed = 0; seed < 25; seed++)
            {
                var playlist = BuildPlaylist(8, PlaybackMode.Shuffle);
                var order = new PlaybackOrder(new Random(seed));

                // Mode picked in the UI: nothing is playing, so there is nothing to anchor to.
                order.Rebuild(playlist, -1);

                // Play pressed on a track - that is where the pass begins.
                const int started = 3;
                order.Rebuild(playlist, started);

                var played = Walk(order, playlist, started, 30);

                Assert.That(played, Is.EquivalentTo(new[] { 0, 1, 2, 3, 4, 5, 6, 7 }), $"seed {seed} played {played.Count} of 8");
            }
        }

        [Test]
        public void ShuffleLoop_KeepsGoingAndNeverRepeatsAcrossTheSeam()
        {
            var playlist = BuildPlaylist(5, PlaybackMode.ShuffleLoop);
            var order = new PlaybackOrder(new Random(5));
            order.Rebuild(playlist, 0);

            var played = Walk(order, playlist, 0, 16);

            // The entry it started on, plus every one of the 16 steps: a looping mode must not run out.
            Assert.AreEqual(17, played.Count, "the chain stopped before the step cap");
            Assert.That(played.GetRange(0, 5), Is.EquivalentTo(new[] { 0, 1, 2, 3, 4 }), "the first pass skipped an entry");

            for (var i = 1; i < played.Count; i++)
                Assert.AreNotEqual(played[i - 1], played[i], $"entry {played[i]} played twice in a row at step {i}");
        }

        // The reason the order is materialised up front rather than drawn one track at a time: Prev in a
        // shuffled mode is only meaningful if there is an order to walk back along.
        [Test]
        public void Shuffle_PreviousRetracesTheOrderItCameBy()
        {
            var playlist = BuildPlaylist(6, PlaybackMode.Shuffle);
            var order = new PlaybackOrder(new Random(23));
            order.Rebuild(playlist, 0);

            var played = Walk(order, playlist, 0, 4);

            for (var i = played.Count - 1; i > 0; i--)
                Assert.AreEqual(played[i - 1], order.Previous(playlist, played[i]), $"step {i} went back to the wrong entry");
        }

        [Test]
        public void AnEntryThatIsNotInTheOrderStartsFromTheTop()
        {
            var playlist = BuildPlaylist(3);
            var order = new PlaybackOrder();

            // What the transport hits when the sidebar's selected playlist is not the one playing.
            Assert.AreEqual(0, order.Next(playlist, -1, isAutoAdvance: true));
            Assert.AreEqual(0, order.Previous(playlist, -1));
        }

        [Test]
        public void AnEmptyPlaylistHasNowhereToGo()
        {
            var playlist = BuildPlaylist(0);
            var order = new PlaybackOrder();

            Assert.IsNull(order.Next(playlist, -1, isAutoAdvance: true));
            Assert.IsNull(order.Previous(playlist, -1));
            Assert.That(order.Upcoming(playlist, -1, 0), Is.Empty);
            Assert.That(order.PlanPrewarm(playlist, -1), Is.Empty);
        }

        // The owner is expected to call Rebuild when it edits the entries. This is the backstop for when
        // it doesn't - without it the order still holds indices past the end of the playlist.
        [Test]
        public void AChangedEntryCountRebuildsWithoutBeingAsked()
        {
            var playlist = BuildPlaylist(3);
            var order = new PlaybackOrder();
            order.Rebuild(playlist, 0);

            playlist.Entries.RemoveAt(2);

            var next = order.Next(playlist, 0, isAutoAdvance: true);

            Assert.AreEqual(1, next);
            Assert.IsNull(order.Next(playlist, 1, isAutoAdvance: true), "the order still pointed past the end");
        }

        [Test]
        public void Upcoming_AnswersWhatWillActuallyPlay()
        {
            var sequential = BuildPlaylist(4);
            var order = new PlaybackOrder();

            Assert.That(order.Upcoming(sequential, 1, 0), Is.EqualTo(new[] { sequential.Entries[2], sequential.Entries[3] }));
            Assert.That(order.Upcoming(sequential, 1, 1), Is.EqualTo(new[] { sequential.Entries[2] }));

            var single = BuildPlaylist(4, PlaybackMode.Single);
            Assert.That(new PlaybackOrder().Upcoming(single, 1, 0), Is.Empty, "Single has no next track");

            var repeatOne = BuildPlaylist(4, PlaybackMode.RepeatOne);
            Assert.That(new PlaybackOrder().Upcoming(repeatOne, 1, 0), Is.EqualTo(new[] { repeatOne.Entries[1] }));
        }

        [Test]
        public void Upcoming_WrapsInALoopingModeButStopsShortOfTheCurrentEntry()
        {
            var playlist = BuildPlaylist(3, PlaybackMode.RepeatAll);

            Assert.That(
                new PlaybackOrder().Upcoming(playlist, 2, 0),
                Is.EqualTo(new[] { playlist.Entries[0], playlist.Entries[1] }));
        }

        [Test]
        public void Recent_IsWhatPlayedBeforeTheCursor_NewestFirst()
        {
            var playlist = BuildPlaylist(4);
            var order = new PlaybackOrder();

            Assert.That(order.Recent(playlist, 2, 0), Is.EqualTo(new[] { playlist.Entries[1], playlist.Entries[0] }));
            Assert.That(order.Recent(playlist, 2, 1), Is.EqualTo(new[] { playlist.Entries[1] }));
            Assert.That(order.Recent(playlist, 0, 0), Is.Empty);
        }

        // The prewarmer prunes its temp folder against the set the pass prepared, so a plan that misses an
        // entry deletes a copy that is still live.
        [Test]
        public void PlanPrewarm_CoversEveryEntryOnceAndLeadsWithWhatPlaysNext()
        {
            var playlist = BuildPlaylist(4);

            Assert.That(
                new PlaybackOrder().PlanPrewarm(playlist, 2),
                Is.EqualTo(new[] { playlist.Entries[3], playlist.Entries[0], playlist.Entries[1], playlist.Entries[2] }));
        }

        [Test]
        public void PlanPrewarm_LeadsWithTheCurrentEntryInRepeatOne()
        {
            var playlist = BuildPlaylist(4, PlaybackMode.RepeatOne);
            var plan = new PlaybackOrder().PlanPrewarm(playlist, 2);

            Assert.AreSame(playlist.Entries[2], plan[0]);
            Assert.AreEqual(4, plan.Count);
        }

        [Test]
        public void PlanPrewarm_FollowsTheShuffledOrder()
        {
            var playlist = BuildPlaylist(6, PlaybackMode.Shuffle);
            var order = new PlaybackOrder(new Random(53));
            order.Rebuild(playlist, 0);

            var next = order.Next(playlist, 0, isAutoAdvance: true);
            var plan = order.PlanPrewarm(playlist, 0);

            Assert.AreSame(playlist.Entries[next.Value], plan[0], "the file the game will ask for next is not first in line");
            Assert.AreEqual(6, plan.Count);
        }

        // Follows the auto-advance chain, collecting the indices it lands on, and gives up once the mode
        // says to stop or the cap is reached (a looping mode never stops on its own).
        private static List<int> Walk(PlaybackOrder order, Playlist playlist, int from, int steps)
        {
            var played = new List<int> { from };
            var current = from;

            for (var i = 0; i < steps; i++)
            {
                var next = order.Next(playlist, current, isAutoAdvance: true);
                if (!next.HasValue)
                    break;

                played.Add(next.Value);
                current = next.Value;
            }

            return played;
        }

        private static Playlist BuildPlaylist(int count, PlaybackMode mode = PlaybackMode.Sequential)
        {
            var playlist = new Playlist { Name = "Order", Mode = mode };
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
    }
}
