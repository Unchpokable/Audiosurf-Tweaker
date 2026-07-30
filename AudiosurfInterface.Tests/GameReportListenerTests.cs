namespace AudiosurfInterface.Tests
{
    using NUnit.Framework;

    // What asbridge forwards is the game's raw WM_COPYDATA line, "asreport " prefix included; the
    // bare-key cases are here only to pin that stripping the prefix is not what makes the rest of the
    // parse work, so a future normalization upstream wouldn't break this class.
    [TestFixture]
    public class GameReportListenerTests
    {
        [TestCase("asreport songcomplete 12345")]
        [TestCase("songcomplete 12345")]
        public void SongComplete_IsParsedFromTheForwardedReportLine(string payload)
        {
            using var listener = new GameReportListener();
            int? receivedScore = null;
            listener.SongCompleted += score => receivedScore = score;

            listener.ProcessReport(payload);

            Assert.AreEqual(12345, receivedScore);
        }

        [TestCase("asreport oncharacterscreen")]
        [TestCase("oncharacterscreen")]
        public void OnCharacterScreen_IsParsedFromTheForwardedReportLine(string payload)
        {
            using var listener = new GameReportListener();
            var received = false;
            listener.OnCharacterScreen += () => received = true;

            listener.ProcessReport(payload);

            Assert.IsTrue(received);
        }

        [Test]
        public void NowPlaying_IsRaisedOnceAllThreeReportsHaveArrived()
        {
            using var listener = new GameReportListener();
            (string Artist, string Title, string AshFile)? received = null;
            listener.NowPlaying += (artist, title, ashFile) => received = (artist, title, ashFile);

            listener.ProcessReport("asreport nowplayingartistname nine inch nails");
            Assert.IsNull(received);

            listener.ProcessReport("asreport nowplayingsongtitle just like you imagined");
            Assert.IsNull(received);

            listener.ProcessReport(@"asreport nowplayingashfile C:\Audiosurf\104415601 - SomeSong.mp3.ash");

            Assert.IsNotNull(received);
            Assert.AreEqual("nine inch nails", received.Value.Artist);
            Assert.AreEqual("just like you imagined", received.Value.Title);
            Assert.AreEqual(@"C:\Audiosurf\104415601 - SomeSong.mp3.ash", received.Value.AshFile);
        }

        [Test]
        public void SongComplete_WithInvalidScore_IsIgnored()
        {
            using var listener = new GameReportListener();
            var received = false;
            listener.SongCompleted += _ => received = true;

            listener.ProcessReport("asreport songcomplete nope");

            Assert.IsFalse(received);
        }
    }
}
