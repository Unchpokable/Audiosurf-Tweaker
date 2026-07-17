namespace QuickPlayerCore.Tests
{
    using System;
    using AudiosurfInterface;
    using NUnit.Framework;
    using QuickPlayerCore.Audiosurf;

    [TestFixture]
    public class SongTagCatalogTests
    {
        [TestCase(SongTagToken.FourLanes, "[as-4lane]")]
        [TestCase(SongTagToken.Portal, "[as-portal]")]
        [TestCase(SongTagToken.MonoOnly, "[as-monoonly]")]
        [TestCase(SongTagToken.EverybodyMono, "[as-everybodymono]")]
        [TestCase(SongTagToken.MonoLessGrey, "[as-lessgrey]")]
        [TestCase(SongTagToken.MonoAllGrey, "[as-allgrey]")]
        [TestCase(SongTagToken.MonoNoGrey, "[as-nogrey]")]
        [TestCase(SongTagToken.NoStealth, "[as-nostlth]")]
        [TestCase(SongTagToken.SidewinderCamera, "[as-swind]")]
        [TestCase(SongTagToken.BankingCamera, "[as-bankcam]")]
        [TestCase(SongTagToken.FirstPerson, "[as-first]")]
        [TestCase(SongTagToken.Caterpillar, "[as-caterp]")]
        [TestCase(SongTagToken.HidePuzzleGrid, "[as-hidepuz]")]
        [TestCase(SongTagToken.Steep, "[as-steep]")]
        public void Format_NoParameterTags_MatchesDocumentedKeyword(SongTagToken token, string expected)
        {
            Assert.AreEqual(expected, SongTagCatalog.Get(token).Format());
        }

        [Test]
        public void Format_MinimumMatchSize_UsesParameter()
        {
            Assert.AreEqual("[as-msz12]", SongTagCatalog.Get(SongTagToken.MinimumMatchSize).Format(12));
        }

        [Test]
        public void Format_WhitesBlacksPercent_UsesParameter()
        {
            Assert.AreEqual("[as-wb50]", SongTagCatalog.Get(SongTagToken.WhitesBlacksPercent).Format(50));
        }

        [Test]
        public void Format_MonoBasePoints_UsesParameter()
        {
            Assert.AreEqual("[as-monopt3]", SongTagCatalog.Get(SongTagToken.MonoBasePoints).Format(3));
        }

        [Test]
        public void Format_PuzzleRowsCount_UsesParameter()
        {
            Assert.AreEqual("[as-prows4]", SongTagCatalog.Get(SongTagToken.PuzzleRowsCount).Format(4));
        }

        [Test]
        public void Format_MatchCollectionTicks_UsesParameter()
        {
            Assert.AreEqual("[as-mt15]", SongTagCatalog.Get(SongTagToken.MatchCollectionTicks).Format(15));
        }

        [Test]
        public void Format_MinimumMatchSize_OutOfRange_Throws()
        {
            Assert.Throws<ArgumentOutOfRangeException>(() => SongTagCatalog.Get(SongTagToken.MinimumMatchSize).Format(25));
        }

        [Test]
        public void Format_WhitesBlacksPercent_OutOfRange_Throws()
        {
            Assert.Throws<ArgumentOutOfRangeException>(() => SongTagCatalog.Get(SongTagToken.WhitesBlacksPercent).Format(101));
        }

        [Test]
        public void Format_ParameterizedTag_MissingParameter_Throws()
        {
            Assert.Throws<ArgumentException>(() => SongTagCatalog.Get(SongTagToken.WhitesBlacksPercent).Format());
        }

        [Test]
        public void SidewinderCamera_HasAsConfigBinding()
        {
            var binding = SongTagCatalog.Get(SongTagToken.SidewinderCamera).AsConfigBinding;
            Assert.IsTrue(binding.HasValue);
            Assert.AreEqual(GameProtocol.Sidewinder, binding.Value.ConfigKey);
            Assert.IsTrue(binding.Value.Value);
        }

        [Test]
        public void BankingCamera_HasAsConfigBinding()
        {
            var binding = SongTagCatalog.Get(SongTagToken.BankingCamera).AsConfigBinding;
            Assert.IsTrue(binding.HasValue);
            Assert.AreEqual(GameProtocol.UseBankingCamera, binding.Value.ConfigKey);
            Assert.IsTrue(binding.Value.Value);
        }

        [Test]
        public void FourLanes_HasNoAsConfigBinding()
        {
            Assert.IsNull(SongTagCatalog.Get(SongTagToken.FourLanes).AsConfigBinding);
        }

        [Test]
        public void All_ContainsEveryToken()
        {
            foreach (SongTagToken token in Enum.GetValues(typeof(SongTagToken)))
                Assert.IsNotNull(SongTagCatalog.Get(token), $"Missing catalog entry for {token}");
        }
    }
}
