namespace QuickPlayerCore.Tests
{
    using System.Linq;
    using NUnit.Framework;
    using QuickPlayerCore.Audiosurf;

    [TestFixture]
    public class SongTitleTagParserTests
    {
        [Test]
        public void PlainTitle_IsLeftAlone()
        {
            var parsed = SongTitleTagParser.Parse("Just Like You Imagined");

            Assert.AreEqual("Just Like You Imagined", parsed.Title);
            Assert.IsEmpty(parsed.Tags);
        }

        [Test]
        public void KnownTag_IsLiftedOutOfTheTitle()
        {
            var parsed = SongTitleTagParser.Parse("Sandstorm [as-4lane]");

            Assert.AreEqual("Sandstorm", parsed.Title);
            Assert.AreEqual(SongTagToken.FourLanes, parsed.Tags.Single().Token);
        }

        [Test]
        public void ParameterizedTag_KeepsItsValue()
        {
            var parsed = SongTitleTagParser.Parse("Sandstorm [as-msz7]");

            Assert.AreEqual("Sandstorm", parsed.Title);
            Assert.AreEqual(SongTagToken.MinimumMatchSize, parsed.Tags.Single().Token);
            Assert.AreEqual(7, parsed.Tags.Single().Parameter);
        }

        [Test]
        public void ParameterOutOfRange_IsNotTreatedAsATag()
        {
            // PuzzleRowsCount accepts 0-8; the game would not have honoured this either, so it stays
            // part of the title rather than being silently clamped or dropped.
            var parsed = SongTitleTagParser.Parse("Sandstorm [as-prows99]");

            Assert.AreEqual("Sandstorm [as-prows99]", parsed.Title);
            Assert.IsEmpty(parsed.Tags);
        }

        [Test]
        public void SeveralTags_AreAllLiftedAndWhitespaceCollapses()
        {
            var parsed = SongTitleTagParser.Parse("Sandstorm [as-4lane] [as-portal] (Radio Edit)");

            Assert.AreEqual("Sandstorm (Radio Edit)", parsed.Title);
            Assert.AreEqual(
                new[] { SongTagToken.FourLanes, SongTagToken.Portal },
                parsed.Tags.Select(t => t.Token).ToArray());
        }

        // The state a hand-edited file lands in after Quick Player appended its own copy. Collapsing it
        // back to one tag is the whole point - the game breaks unpredictably on a duplicate.
        [Test]
        public void RepeatedTag_CollapsesToOne()
        {
            var parsed = SongTitleTagParser.Parse("Sandstorm [as-4lane] [as-4lane]");

            Assert.AreEqual("Sandstorm", parsed.Title);
            Assert.AreEqual(1, parsed.Tags.Count);
        }

        [TestCase("Sandstorm [Remix]", TestName = "Ordinary bracketed text")]
        [TestCase("Sandstorm [AS-4LANE]", TestName = "Wrong case is not a tag the game honours")]
        [TestCase("Sandstorm [as-4lanes]", TestName = "Misspelled")]
        [TestCase("Sandstorm [as-4lane", TestName = "Unclosed")]
        public void UnrecognizedBrackets_StayInTheTitle(string title)
        {
            var parsed = SongTitleTagParser.Parse(title);

            Assert.AreEqual(title, parsed.Title);
            Assert.IsEmpty(parsed.Tags);
        }

        [Test]
        public void UnrecognizedBracket_DoesNotHideARealTagAfterIt()
        {
            var parsed = SongTitleTagParser.Parse("Sandstorm [Remix] [as-portal]");

            Assert.AreEqual("Sandstorm [Remix]", parsed.Title);
            Assert.AreEqual(SongTagToken.Portal, parsed.Tags.Single().Token);
        }

        // Whatever the catalog can write, this has to be able to read back - that is the contract that
        // keeps TempFileTagger from ever composing a duplicate.
        [Test]
        public void EveryCatalogTag_RoundTripsThroughTheParser()
        {
            foreach (var definition in SongTagCatalog.All)
            {
                var parameter = definition.HasParameter ? definition.MinParameter + 1 : (int?)null;
                var rendered = definition.Format(parameter);

                var parsed = SongTitleTagParser.Parse("Song " + rendered);

                Assert.AreEqual("Song", parsed.Title, rendered);
                Assert.AreEqual(1, parsed.Tags.Count, rendered);
                Assert.AreEqual(definition.Token, parsed.Tags[0].Token, rendered);
                Assert.AreEqual(parameter, parsed.Tags[0].Parameter, rendered);
            }
        }

        [Test]
        public void Parse_IsIdempotent()
        {
            var once = SongTitleTagParser.Parse("Sandstorm [as-4lane] [as-msz7]");
            var twice = SongTitleTagParser.Parse(once.Title);

            Assert.AreEqual(once.Title, twice.Title);
            Assert.IsEmpty(twice.Tags);
        }
    }
}
