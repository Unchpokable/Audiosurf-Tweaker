namespace QuickPlayerCore.Tests
{
    using System;
    using System.Collections.Generic;
    using System.IO;
    using System.Text;
    using System.Threading;
    using NUnit.Framework;
    using QuickPlayerCore.Audiosurf;

    [TestFixture]
    public class TempFileTaggerTests
    {
        private string _tempDir;
        private readonly List<Guid> _playlistIds = new();

        [SetUp]
        public void SetUp()
        {
            _tempDir = Path.Combine(Path.GetTempPath(), "QuickPlayerCoreTests", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(_tempDir);
        }

        [TearDown]
        public void TearDown()
        {
            if (Directory.Exists(_tempDir))
                Directory.Delete(_tempDir, recursive: true);

            // The prepared copies land under QuickPlayer/Temp next to the test binary, not in _tempDir.
            foreach (var playlistId in _playlistIds)
                TempFileTagger.DropTempDirectory(playlistId);
            _playlistIds.Clear();
        }

        private Guid NewPlaylistId()
        {
            var id = Guid.NewGuid();
            _playlistIds.Add(id);
            return id;
        }

        [Test]
        public void ResolvePlaybackPath_NoActiveTitleTags_ReturnsOriginalPath()
        {
            var source = CreateSampleWav("plain.wav");
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "Plain Song" };

            var result = TempFileTagger.ResolvePlaybackPath(NewPlaylistId(), entry);

            Assert.AreEqual(source, result);
        }

        [Test]
        public void ResolvePlaybackPath_OnlyAsConfigBoundTag_ReturnsOriginalPath()
        {
            var source = CreateSampleWav("configonly.wav");
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "Plain Song" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.SidewinderCamera });

            var result = TempFileTagger.ResolvePlaybackPath(NewPlaylistId(), entry);

            Assert.AreEqual(source, result);
        }

        [Test]
        public void ResolvePlaybackPath_ActiveTitleTag_CreatesCopyWithRewrittenTitle()
        {
            var source = CreateSampleWav("tagged.wav");
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "My Song" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            var result = TempFileTagger.ResolvePlaybackPath(NewPlaylistId(), entry);

            Assert.AreNotEqual(source, result);
            Assert.IsTrue(File.Exists(result));

            var tagged = TagLib.File.Create(result);
            Assert.AreEqual("My Song [as-4lane]", tagged.Tag.Title);
        }

        [Test]
        public void ResolvePlaybackPath_RepeatedCallSameTags_DoesNotRewriteCachedCopy()
        {
            var source = CreateSampleWav("cached.wav");
            var playlistId = NewPlaylistId();
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "Cached Song" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            var firstResult = TempFileTagger.ResolvePlaybackPath(playlistId, entry);
            var firstWriteTime = File.GetLastWriteTimeUtc(firstResult);

            Thread.Sleep(50);
            var secondResult = TempFileTagger.ResolvePlaybackPath(playlistId, entry);

            Assert.AreEqual(firstResult, secondResult);
            Assert.AreEqual(firstWriteTime, File.GetLastWriteTimeUtc(secondResult));
        }

        // The copy is content-addressed: a different tag set is a different file, not a rewrite of the
        // old one. That is what lets PlaylistPrewarmer prepare the whole playlist up front - with one
        // copy per source file, two entries of the same song with different tags would overwrite each
        // other and whichever played second would get the wrong tags.
        [Test]
        public void ResolvePlaybackPath_TagsChanged_ProducesASeparateCopy()
        {
            var source = CreateSampleWav("changed.wav");
            var playlistId = NewPlaylistId();
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "Changing Song" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            var firstResult = TempFileTagger.ResolvePlaybackPath(playlistId, entry);
            Assert.AreEqual("Changing Song [as-4lane]", TagLib.File.Create(firstResult).Tag.Title);

            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.Portal });
            var secondResult = TempFileTagger.ResolvePlaybackPath(playlistId, entry);

            Assert.AreNotEqual(firstResult, secondResult);
            Assert.AreEqual("Changing Song [as-4lane][as-portal]", TagLib.File.Create(secondResult).Tag.Title);
            // The first copy survives untouched - it may be the one the game currently has open.
            Assert.AreEqual("Changing Song [as-4lane]", TagLib.File.Create(firstResult).Tag.Title);
        }

        [Test]
        public void ResolvePlaybackPath_SameFileDifferentTags_KeepsBothCopiesIntact()
        {
            var source = CreateSampleWav("twice.wav");
            var playlistId = NewPlaylistId();

            var fourLanes = new PlaylistEntry { FilePath = source, SongTitle = "Twice" };
            fourLanes.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            var portal = new PlaylistEntry { FilePath = source, SongTitle = "Twice" };
            portal.Tags.Add(new PlaylistTag { Token = SongTagToken.Portal });

            var first = TempFileTagger.ResolvePlaybackPath(playlistId, fourLanes);
            var second = TempFileTagger.ResolvePlaybackPath(playlistId, portal);

            Assert.AreNotEqual(first, second);
            Assert.AreEqual("Twice [as-4lane]", TagLib.File.Create(first).Tag.Title);
            Assert.AreEqual("Twice [as-portal]", TagLib.File.Create(second).Tag.Title);
        }

        [Test]
        public void ResolvePlaybackPath_LeavesNoStagingFileBehind()
        {
            var source = CreateSampleWav("staging.wav");
            var playlistId = NewPlaylistId();
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "Staged" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            var result = TempFileTagger.ResolvePlaybackPath(playlistId, entry);

            var produced = Directory.GetFiles(Path.GetDirectoryName(result));
            Assert.AreEqual(new[] { result }, produced);
        }

        [Test]
        public void PruneTempDirectory_RemovesEverythingNotListed()
        {
            var source = CreateSampleWav("prune.wav");
            var playlistId = NewPlaylistId();

            var kept = new PlaylistEntry { FilePath = source, SongTitle = "Prune" };
            kept.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            var dropped = new PlaylistEntry { FilePath = source, SongTitle = "Prune" };
            dropped.Tags.Add(new PlaylistTag { Token = SongTagToken.Portal });

            var keptPath = TempFileTagger.ResolvePlaybackPath(playlistId, kept);
            var droppedPath = TempFileTagger.ResolvePlaybackPath(playlistId, dropped);

            TempFileTagger.PruneTempDirectory(playlistId, new HashSet<string>(new[] { keptPath }, StringComparer.OrdinalIgnoreCase));

            Assert.IsTrue(File.Exists(keptPath));
            Assert.IsFalse(File.Exists(droppedPath));
        }

        // Minimal valid PCM WAV container (RIFF/WAVE/fmt /data) - enough for TagLib to accept the
        // file and round-trip an ID3 title tag, without depending on a real audio fixture on disk.
        private string CreateSampleWav(string fileName)
        {
            var path = Path.Combine(_tempDir, fileName);

            const int sampleRate = 44100;
            const short channels = 1;
            const short bitsPerSample = 16;
            const int sampleCount = 4410;
            var dataSize = sampleCount * channels * (bitsPerSample / 8);

            using (var stream = new FileStream(path, FileMode.Create))
            using (var writer = new BinaryWriter(stream))
            {
                writer.Write(Encoding.ASCII.GetBytes("RIFF"));
                writer.Write(36 + dataSize);
                writer.Write(Encoding.ASCII.GetBytes("WAVE"));

                writer.Write(Encoding.ASCII.GetBytes("fmt "));
                writer.Write(16);
                writer.Write((short)1); // PCM
                writer.Write(channels);
                writer.Write(sampleRate);
                writer.Write(sampleRate * channels * (bitsPerSample / 8));
                writer.Write((short)(channels * (bitsPerSample / 8)));
                writer.Write(bitsPerSample);

                writer.Write(Encoding.ASCII.GetBytes("data"));
                writer.Write(dataSize);
                writer.Write(new byte[dataSize]);
            }

            return path;
        }
    }
}
