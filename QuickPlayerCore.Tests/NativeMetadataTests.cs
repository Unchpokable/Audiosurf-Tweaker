using System;
using System.IO;
using NUnit.Framework;
using QuickPlayerCore.MetadataParsers;

namespace QuickPlayerCore.Tests
{
    public class NativeMetadataTests
    {
        [TestCase("tone.mp3", 0)]
        [TestCase("tone-vbr.mp3", 0)]
        [TestCase("tone.m4a", 16)]
        [TestCase("tone.flac", 16)]
        [TestCase("tone.wav", 16)]
        public void ReadAndRewriteTitlePreservesOtherMetadata(string filename, int bitDepth)
        {
            var source = Path.Combine(TestContext.CurrentContext.TestDirectory, "Fixtures", filename);
            var directory = Path.Combine(Path.GetTempPath(), "tweaker-audio-" + Guid.NewGuid());
            Directory.CreateDirectory(directory);
            var copy = Path.Combine(directory, "Музыка" + Path.GetExtension(filename));
            try
            {
                File.Copy(source, copy);
                var before = NativeAudioMetadata.Read(copy);
                Assert.Multiple(() =>
                {
                    Assert.That(before.Title, Is.EqualTo("Песня Native AOT"));
                    Assert.That(before.Artist, Is.EqualTo("Исполнитель"));
                    Assert.That(before.Album, Is.EqualTo("Альбом"));
                    Assert.That(before.Duration.TotalSeconds, Is.InRange(0.95, 1.1));
                    Assert.That(before.SampleRate, Is.EqualTo(44100));
                    Assert.That(before.BitDepth, Is.EqualTo(bitDepth));
                    Assert.That(before.Bitrate, Is.GreaterThan(0));
                });

                NativeAudioMetadata.SetTitle(copy, "Новый title [as-4lane]");
                var after = NativeAudioMetadata.Read(copy);
                Assert.That(after, Is.EqualTo(before with { Title = "Новый title [as-4lane]" }));
                Assert.That(NativeAudioMetadata.Read(source), Is.EqualTo(before));

                // No native FileRef or Windows file handle may outlive either operation.
                File.Move(copy, copy + ".moved");
            }
            finally
            {
                Directory.Delete(directory, true);
            }
        }

        [Test]
        public void M4aReadsFirstSemicolonSeparatedPerformerAfterTitleRewrite()
        {
            var source = Path.Combine(TestContext.CurrentContext.TestDirectory, "Fixtures", "tone-multiple-artists.m4a");
            var copy = Path.Combine(Path.GetTempPath(), "tweaker-artists-" + Guid.NewGuid() + ".m4a");
            try
            {
                File.Copy(source, copy);
                Assert.That(MetadataReader.ReadData(copy).ArtistName, Is.EqualTo("Первый"));
                NativeAudioMetadata.SetTitle(copy, "title [as-4lane]");
                Assert.That(MetadataReader.ReadData(copy).ArtistName, Is.EqualTo("Первый"));
            }
            finally
            {
                File.Delete(copy);
            }
        }

        [Test]
        public void InvalidFileReportsFailureAndReleasesHandle()
        {
            var path = Path.Combine(Path.GetTempPath(), "tweaker-invalid-" + Guid.NewGuid() + ".mp3");
            try
            {
                File.WriteAllText(path, "not audio");
                Assert.Throws<IOException>(() => NativeAudioMetadata.Read(path));
                Assert.Throws<IOException>(() => NativeAudioMetadata.SetTitle(path, "title"));
            }
            finally
            {
                File.Delete(path);
            }
        }
    }
}
