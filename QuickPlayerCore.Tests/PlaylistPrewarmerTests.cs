namespace QuickPlayerCore.Tests
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Threading;
    using NUnit.Framework;

    // Untagged entries throughout: TempFileTagger then hands the original path straight back, so these
    // exercise the worker's own ordering/validation/reporting without depending on TagLib being able to
    // retag a fixture. The copy-and-retag path itself is covered by TempFileTaggerTests.
    [TestFixture]
    public class PlaylistPrewarmerTests
    {
        private string _tracksDirectory;

        [SetUp]
        public void SetUp()
        {
            _tracksDirectory = Path.Combine(Path.GetTempPath(), "qp-prewarm-" + Guid.NewGuid().ToString("N"));
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

        [Test]
        public void EveryEntryEndsUpReady()
        {
            var playlist = BuildPlaylist(4);
            var recorder = new ProgressRecorder();
            using var prewarmer = new PlaylistPrewarmer(recorder);

            prewarmer.Start(playlist, 0);

            Assert.IsTrue(recorder.WaitForCompletion(), "the pass never finished");
            foreach (var entry in playlist.Entries)
                Assert.AreEqual(EntryReadiness.Ready, prewarmer.GetReadiness(entry), entry.SongTitle);
        }

        // The point of the whole thing: whatever plays next is prepared first, not whatever happens to
        // be at the top of the playlist.
        [Test]
        public void WorkStartsAtTheEntryAfterTheOnePlaying()
        {
            var playlist = BuildPlaylist(4);
            var recorder = new ProgressRecorder();
            using var prewarmer = new PlaylistPrewarmer(recorder);

            prewarmer.Start(playlist, 2);

            Assert.IsTrue(recorder.WaitForCompletion(), "the pass never finished");
            Assert.AreEqual(
                new[]
                {
                    playlist.Entries[3].Id,
                    playlist.Entries[0].Id,
                    playlist.Entries[1].Id,
                    playlist.Entries[2].Id
                },
                recorder.SettledOrder());
        }

        [Test]
        public void AMissingFileIsReportedRatherThanPrepared()
        {
            var playlist = BuildPlaylist(3);
            File.Delete(playlist.Entries[1].FilePath);
            var recorder = new ProgressRecorder();
            using var prewarmer = new PlaylistPrewarmer(recorder);

            prewarmer.Start(playlist, 0);

            Assert.IsTrue(recorder.WaitForCompletion(), "the pass never finished");
            Assert.AreEqual(EntryReadiness.Missing, prewarmer.GetReadiness(playlist.Entries[1]));
            Assert.AreEqual(EntryReadiness.Ready, prewarmer.GetReadiness(playlist.Entries[0]));
            Assert.AreEqual(EntryReadiness.Ready, prewarmer.GetReadiness(playlist.Entries[2]));
        }

        [Test]
        public void ProgressCountsEveryEntryExactlyOnceAndEndsComplete()
        {
            var playlist = BuildPlaylist(3);
            var recorder = new ProgressRecorder();
            using var prewarmer = new PlaylistPrewarmer(recorder);

            prewarmer.Start(playlist, 0);

            Assert.IsTrue(recorder.WaitForCompletion(), "the pass never finished");
            var last = recorder.Last();
            Assert.AreEqual(3, last.Total);
            Assert.AreEqual(3, last.Settled);
            Assert.IsTrue(last.IsComplete);
        }

        [Test]
        public void StopClearsWhatWasLearned()
        {
            var playlist = BuildPlaylist(2);
            var recorder = new ProgressRecorder();
            using var prewarmer = new PlaylistPrewarmer(recorder);

            prewarmer.Start(playlist, 0);
            Assert.IsTrue(recorder.WaitForCompletion(), "the pass never finished");

            prewarmer.Stop();

            Assert.AreEqual(EntryReadiness.Unknown, prewarmer.GetReadiness(playlist.Entries[0]));
        }

        // Called on every track change; restarting the pass each time would throw away everything
        // already prepared and begin again from the new index.
        [Test]
        public void StartingAgainForTheSamePlaylistDoesNotRestartTheRunningPass()
        {
            var playlist = BuildPlaylist(3);
            var recorder = new ProgressRecorder();
            using var prewarmer = new PlaylistPrewarmer(recorder);

            prewarmer.Start(playlist, 0);
            Assert.IsTrue(recorder.WaitForCompletion(), "the pass never finished");
            var reportsAfterFirstPass = recorder.Count;

            prewarmer.Start(playlist, 1);
            Thread.Sleep(100);

            // The first pass has finished, so this one is allowed to run - what must not happen is the
            // readiness map being wiped for a playlist that is still the current one.
            foreach (var entry in playlist.Entries)
                Assert.AreNotEqual(EntryReadiness.Unknown, prewarmer.GetReadiness(entry), entry.SongTitle);
            Assert.GreaterOrEqual(recorder.Count, reportsAfterFirstPass);
        }

        [Test]
        public void AnEmptyPlaylistSettlesImmediately()
        {
            var playlist = new Playlist { Name = "Empty" };
            var recorder = new ProgressRecorder();
            using var prewarmer = new PlaylistPrewarmer(recorder);

            Assert.DoesNotThrow(() => prewarmer.Start(playlist, 0));
        }

        private Playlist BuildPlaylist(int count)
        {
            var playlist = new Playlist { Name = "Prewarm" };
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

        private sealed class ProgressRecorder : IProgress<PrewarmProgress>
        {
            private readonly object _gate = new();
            private readonly List<PrewarmProgress> _reports = new();
            private readonly ManualResetEventSlim _completed = new(false);

            public int Count
            {
                get
                {
                    lock (_gate)
                        return _reports.Count;
                }
            }

            public void Report(PrewarmProgress value)
            {
                lock (_gate)
                    _reports.Add(value);

                if (value.IsComplete)
                    _completed.Set();
            }

            public bool WaitForCompletion(int timeoutMs = 5000) => _completed.Wait(timeoutMs);

            public PrewarmProgress Last()
            {
                lock (_gate)
                    return _reports[_reports.Count - 1];
            }

            /// <summary>Entry ids in the order they reached a final state, one per entry.</summary>
            public Guid[] SettledOrder()
            {
                lock (_gate)
                {
                    var seen = new List<Guid>();
                    foreach (var report in _reports)
                    {
                        if (report.Readiness == EntryReadiness.Preparing || seen.Contains(report.EntryId))
                            continue;
                        seen.Add(report.EntryId);
                    }

                    return seen.ToArray();
                }
            }
        }
    }
}
