namespace QuickPlayerCore.Tests
{
    using NUnit.Framework;
    using QuickPlayerCore.PackedPresenters;

    [TestFixture]
    public class SupportedAudioFormatsTests
    {
        [TestCase("song.mp3", true)]
        [TestCase("song.MP3", true)]
        [TestCase("song.flac", true)]
        [TestCase("song.m4a", true)]
        [TestCase("song.wav", true)]
        [TestCase("song.xyz", false)]
        [TestCase("song", false)]
        public void IsSupported_MatchesExpected(string fileName, bool expected)
        {
            Assert.AreEqual(expected, SupportedAudioFormats.IsSupported(fileName));
        }

        [Test]
        public void TryGetCodec_KnownExtension_ReturnsMatchingCodec()
        {
            Assert.IsTrue(SupportedAudioFormats.TryGetCodec("song.flac", out var codec));
            Assert.AreEqual(Codec.Flac, codec);
        }

        [Test]
        public void TryGetCodec_UnknownExtension_ReturnsUnsupported()
        {
            Assert.IsFalse(SupportedAudioFormats.TryGetCodec("song.xyz", out var codec));
            Assert.AreEqual(Codec.Unsupported, codec);
        }

        [Test]
        public void Register_AddsNewExtensionAtRuntime()
        {
            Assert.IsFalse(SupportedAudioFormats.IsSupported("song.ogg"));

            SupportedAudioFormats.Register("ogg", Codec.Ogg);

            Assert.IsTrue(SupportedAudioFormats.IsSupported("song.ogg"));
        }
    }
}
