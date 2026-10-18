namespace TweakerCore.Tests.Scripting
{
    using NUnit.Framework;
    using TweakerCore.Scripting;

    /// <summary>
    /// The run order as the plugin builds it at startup (TweakerPlugin/harness/remote checks the same rule on the
    /// plugin's side), and one name moved within it.
    /// </summary>
    [TestFixture]
    public class ScriptOrderTests
    {
        [Test]
        public void ListedNamesComeFirstInTheirOrderThenTheRestByName()
        {
            var order = ScriptOrder.Arrange(new[] { "a.lua", "b.lua", "c.lua", "d.lua", "B.lua" }, new[] { "c.lua", "ghost.lua", "a.lua" });

            Assert.That(order, Is.EqualTo(new[] { "c.lua", "a.lua", "B.lua", "b.lua", "d.lua" }));
        }

        [Test]
        public void NoOrderIsByNameOrdinal()
        {
            Assert.That(ScriptOrder.Arrange(new[] { "b.lua", "a.lua", "B.lua" }, null), Is.EqualTo(new[] { "B.lua", "a.lua", "b.lua" }));
        }

        [Test]
        public void NamesCompareCaseSensitivelyLikeThePlugin()
        {
            Assert.That(ScriptOrder.Arrange(new[] { "a.lua", "b.lua" }, new[] { "B.lua" }), Is.EqualTo(new[] { "a.lua", "b.lua" }));
        }

        [TestCase("c", "a", new[] { "c", "a", "b", "d" })]
        [TestCase("a", "d", new[] { "b", "c", "a", "d" })]
        [TestCase("a", null, new[] { "b", "c", "d", "a" })]
        [TestCase("d", "a", new[] { "d", "a", "b", "c" })]
        [TestCase("b", "c", new[] { "a", "b", "c", "d" })]
        [TestCase("b", "gone", new[] { "a", "c", "d", "b" })]
        public void MovePutsTheNameRightBeforeAnother(string file, string before, string[] expected)
        {
            Assert.That(ScriptOrder.Move(new[] { "a", "b", "c", "d" }, file, before), Is.EqualTo(expected));
        }

        [Test]
        public void MoveKeepsNamesTheCallerDoesNotShow()
        {
            // "gone.lua" is a script whose file went away while the game ran: the plugin keeps its place, and the
            // Scripts tab, which has no row for it, must not move it by moving something else.
            var order = ScriptOrder.Move(new[] { "a.lua", "gone.lua", "b.lua", "c.lua" }, "c.lua", "a.lua");

            Assert.That(order, Is.EqualTo(new[] { "c.lua", "a.lua", "gone.lua", "b.lua" }));
        }
    }
}
