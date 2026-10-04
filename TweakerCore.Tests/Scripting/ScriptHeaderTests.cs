namespace TweakerCore.Tests.Scripting
{
    using System.IO;
    using System.Text;
    using NUnit.Framework;
    using TweakerCore.Scripting;

    /// <summary>
    /// ScriptHeader against the behaviour of the plugin's read_header (TweakerPlugin/src/lua/lua_script.cxx) -
    /// every case here is a rule of that function, the host's list has to name a script as the overlay does.
    /// </summary>
    [TestFixture]
    public class ScriptHeaderTests
    {
        private ScratchGame _game;

        [SetUp]
        public void SetUp() => _game = new ScratchGame();

        [TearDown]
        public void TearDown() => _game.Dispose();

        private ScriptHeader Read(string text)
        {
            _game.Script("demo.lua", text);
            return ScriptHeader.Read(_game.ScriptPath("demo.lua"));
        }

        [Test]
        public void ReadsEveryKey()
        {
            var header = Read("-- @name Demo HUD\n-- @author me\n-- @version 1.2.3\n-- @description Shows things.\n-- @api 2\n-- @bundle twllink 1.1, 3 modules\nlocal x = 1\n");

            Assert.That(header.Name, Is.EqualTo("Demo HUD"));
            Assert.That(header.Author, Is.EqualTo("me"));
            Assert.That(header.Version, Is.EqualTo("1.2.3"));
            Assert.That(header.Description, Is.EqualTo("Shows things."));
            Assert.That(header.Api, Is.EqualTo(2));
            Assert.That(header.Bundle, Is.EqualTo("twllink 1.1, 3 modules"));
            Assert.That(header.IsBundle, Is.True);
        }

        [Test]
        public void WithoutAHeaderTheNameIsTheFileName()
        {
            var header = Read("local x = 1\n");

            Assert.That(header.Name, Is.EqualTo("demo"));
            Assert.That(header.Api, Is.EqualTo(0));
            Assert.That(header.IsBundle, Is.False);
        }

        [Test]
        public void StopsAtTheFirstLineOfCode()
        {
            var header = Read("-- @name Demo\n\n-- a comment\nlocal x = 1\n-- @author too late\n");

            Assert.That(header.Name, Is.EqualTo("Demo"));
            Assert.That(header.Author, Is.Empty);
        }

        [Test]
        public void AcceptsExtraDashesAndSpacingAndCrLf()
        {
            var header = Read("--- @name   Spaced out \r\n--@author\tTab\r\n");

            Assert.That(header.Name, Is.EqualTo("Spaced out"));
            Assert.That(header.Author, Is.EqualTo("Tab"));
        }

        [TestCase("-- @api two")]
        [TestCase("-- @api 0")]
        [TestCase("-- @api -1")]
        [TestCase("-- @api 1.5")]
        public void AnApiThatIsNotAWholePositiveNumberIsIgnored(string line)
        {
            Assert.That(Read("-- @api 3\n" + line + "\n").Api, Is.EqualTo(3));
        }

        [Test]
        public void AKeyWithoutAValueIsSkipped()
        {
            Assert.That(Read("-- @name\n").Name, Is.EqualTo("demo"));
        }

        [Test]
        public void AByteOrderMarkHidesTheHeaderAsItDoesInThePlugin()
        {
            File.WriteAllText(_game.ScriptPath("demo.lua"), "-- @name Demo\n", new UTF8Encoding(true));

            Assert.That(ScriptHeader.Read(_game.ScriptPath("demo.lua")).Name, Is.EqualTo("demo"));
        }

        [Test]
        public void AMissingFileGivesOnlyTheName()
        {
            Assert.That(ScriptHeader.Read(_game.ScriptPath("gone.lua")).Name, Is.EqualTo("gone"));
        }
    }
}
