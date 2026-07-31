namespace QuickPlayerCore.Tests
{
    using System.Linq;
    using AudiosurfInterface;
    using NUnit.Framework;
    using QuickPlayerCore.Audiosurf;

    // Tags the user wrote into the file themselves have to end up as entry state, not stay in the
    // title - TempFileTagger composes title + tags on playback, and a duplicated tag breaks the game.
    [TestFixture]
    public class PlaylistEntryTagAdoptionTests
    {
        [Test]
        public void TitleTag_MovesIntoTheEntrysTags()
        {
            var entry = new PlaylistEntry { SongTitle = "Sandstorm [as-4lane]" };

            entry.AdoptTagsFromTitle();

            Assert.AreEqual("Sandstorm", entry.SongTitle);
            Assert.AreEqual(SongTagToken.FourLanes, entry.Tags.Single().Token);
        }

        // Sidewinder/BankingCamera are shown as Mods, and TagOptionViewModel hides them from the Tags
        // list - parking them in Tags would get them wiped the first time the user edited the track.
        [Test]
        public void AsConfigBoundTitleTag_BecomesAModOverrideNotATag()
        {
            var entry = new PlaylistEntry { SongTitle = "Sandstorm [as-swind]" };

            entry.AdoptTagsFromTitle();

            Assert.AreEqual("Sandstorm", entry.SongTitle);
            Assert.IsEmpty(entry.Tags);
            Assert.IsTrue(entry.ConfigOverrides[GameProtocol.Sidewinder]);
        }

        [Test]
        public void AlreadyPresentTag_IsNotDuplicated()
        {
            var entry = new PlaylistEntry { SongTitle = "Sandstorm [as-4lane]" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.FourLanes });

            entry.AdoptTagsFromTitle();

            Assert.AreEqual("Sandstorm", entry.SongTitle);
            Assert.AreEqual(1, entry.Tags.Count);
        }

        [Test]
        public void CleanTitle_IsUntouched()
        {
            var entry = new PlaylistEntry { SongTitle = "Sandstorm (Radio Edit)" };
            entry.Tags.Add(new PlaylistTag { Token = SongTagToken.Portal });

            entry.AdoptTagsFromTitle();

            Assert.AreEqual("Sandstorm (Radio Edit)", entry.SongTitle);
            Assert.AreEqual(SongTagToken.Portal, entry.Tags.Single().Token);
        }

        // Runs on every playlist load, so it has to be safe to apply over and over.
        [Test]
        public void Adoption_IsIdempotent()
        {
            var entry = new PlaylistEntry { SongTitle = "Sandstorm [as-4lane] [as-msz7]" };

            entry.AdoptTagsFromTitle();
            var afterFirst = entry.SongTitle;
            entry.AdoptTagsFromTitle();

            Assert.AreEqual(afterFirst, entry.SongTitle);
            Assert.AreEqual(2, entry.Tags.Count);
            Assert.AreEqual(7, entry.Tags.Single(t => t.Token == SongTagToken.MinimumMatchSize).Parameter);
        }

        // The exact shape a hand-tagged file lands in once Quick Player has appended its own copy.
        [Test]
        public void DuplicatedTitleTag_CollapsesToASingleTag()
        {
            var entry = new PlaylistEntry { SongTitle = "Sandstorm [as-4lane] [as-4lane]" };

            entry.AdoptTagsFromTitle();

            Assert.AreEqual("Sandstorm", entry.SongTitle);
            Assert.AreEqual(1, entry.Tags.Count);
        }
    }
}
