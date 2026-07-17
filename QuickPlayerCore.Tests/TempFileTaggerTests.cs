namespace QuickPlayerCore.Tests
{
    using System;
    using System.IO;
    using System.Text;
    using System.Threading;
    using NUnit.Framework;
    using QuickPlayerCore.Audiosurf;

    [TestFixture]
    public class TempFileTaggerTests
    {
        private string _tempDir;

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
        }

        [Test]
        public void ResolvePlaybackPath_NoActiveTitleTags_ReturnsOriginalPath()
        {
            var source = CreateSampleWav("plain.wav");
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "Plain Song" };

            var result = TempFileTagger.ResolvePlaybackPath(Guid.NewGuid(), entry);

            Assert.AreEqual(source, result);
        }

        [Test]
        public void ResolvePlaybackPath_OnlyAsConfigBoundTag_ReturnsOriginalPath()
        {
            var source = CreateSampleWav("configonly.wav");
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "Plain Song" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.SidewinderCamera });

            var result = TempFileTagger.ResolvePlaybackPath(Guid.NewGuid(), entry);

            Assert.AreEqual(source, result);
        }

        [Test]
        public void ResolvePlaybackPath_ActiveTitleTag_CreatesCopyWithRewrittenTitle()
        {
            var source = CreateSampleWav("tagged.wav");
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "My Song" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            var result = TempFileTagger.ResolvePlaybackPath(Guid.NewGuid(), entry);

            Assert.AreNotEqual(source, result);
            Assert.IsTrue(File.Exists(result));

            var tagged = TagLib.File.Create(result);
            Assert.AreEqual("My Song [as-4lane]", tagged.Tag.Title);
        }

        [Test]
        public void ResolvePlaybackPath_RepeatedCallSameTags_DoesNotRewriteCachedCopy()
        {
            var source = CreateSampleWav("cached.wav");
            var playlistId = Guid.NewGuid();
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "Cached Song" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            var firstResult = TempFileTagger.ResolvePlaybackPath(playlistId, entry);
            var firstWriteTime = File.GetLastWriteTimeUtc(firstResult);

            Thread.Sleep(50);
            var secondResult = TempFileTagger.ResolvePlaybackPath(playlistId, entry);

            Assert.AreEqual(firstResult, secondResult);
            Assert.AreEqual(firstWriteTime, File.GetLastWriteTimeUtc(secondResult));
        }

        [Test]
        public void ResolvePlaybackPath_TagsChanged_RewritesCachedCopy()
        {
            var source = CreateSampleWav("changed.wav");
            var playlistId = Guid.NewGuid();
            var entry = new PlaylistEntry { FilePath = source, SongTitle = "Changing Song" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            var firstResult = TempFileTagger.ResolvePlaybackPath(playlistId, entry);
            Assert.AreEqual("Changing Song [as-4lane]", TagLib.File.Create(firstResult).Tag.Title);

            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.Portal });
            var secondResult = TempFileTagger.ResolvePlaybackPath(playlistId, entry);

            Assert.AreEqual(firstResult, secondResult);
            Assert.AreEqual("Changing Song [as-4lane][as-portal]", TagLib.File.Create(secondResult).Tag.Title);
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
