namespace QuickPlayerCore.Tests
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using AudiosurfInterface;
    using NUnit.Framework;

    // AudiosurfHandle/GameConfigState are process-wide singletons (the game gives no way to read
    // config back, so there is no seam to mock through) - tests use unique per-test key names to
    // avoid interfering with each other, and observe outbound commands via CommandSent rather than
    // a real game/bridge connection.
    [TestFixture]
    public class GameConfigStateTests
    {
        private List<string> _sentCommands;
        private EventHandler<CommandInfo> _handler;

        [SetUp]
        public void SetUp()
        {
            _sentCommands = new List<string>();
            _handler = (_, info) => _sentCommands.Add(info.CommandText);
            AudiosurfHandle.Instance.CommandSent += _handler;
        }

        [TearDown]
        public void TearDown()
        {
            AudiosurfHandle.Instance.CommandSent -= _handler;
        }

        [Test]
        public void PushOverride_Dispose_RestoresPreviousKnownValue()
        {
            var key = UniqueKey();
            GameConfigState.Manager.Set(key, false);
            _sentCommands.Clear();

            using (GameConfigState.Manager.PushOverride(key, true))
            {
                Assert.AreEqual(new[] { $"asconfig {key} true" }, _sentCommands);
            }

            Assert.AreEqual($"asconfig {key} false", _sentCommands.Last());
        }

        [Test]
        public void PushOverride_TwoKeys_DisposeRestoresEachIndependently()
        {
            var keyA = UniqueKey();
            var keyB = UniqueKey();
            GameConfigState.Manager.Set(keyA, false);
            GameConfigState.Manager.Set(keyB, true);
            _sentCommands.Clear();

            var handleA = GameConfigState.Manager.PushOverride(keyA, true);
            var handleB = GameConfigState.Manager.PushOverride(keyB, false);

            // Dispose in LIFO order, as PlaybackController.EndCurrent does.
            handleB.Dispose();
            handleA.Dispose();

            var restores = _sentCommands.Skip(2).ToList();
            Assert.AreEqual($"asconfig {keyB} true", restores[0]);
            Assert.AreEqual($"asconfig {keyA} false", restores[1]);
        }

        [Test]
        public void PushOverride_NoPriorValue_DisposeRestoresToGameDefaultFalse()
        {
            // Every asconfig key here is a toggle the game itself defaults to off - if nobody ever
            // set this key before, "off" is the truthful value to restore to, not "leave it alone".
            var key = UniqueKey();
            _sentCommands.Clear();

            using (GameConfigState.Manager.PushOverride(key, true))
            {
                Assert.AreEqual(new[] { $"asconfig {key} true" }, _sentCommands);
            }

            Assert.AreEqual($"asconfig {key} false", _sentCommands.Last());
        }

        [Test]
        public void PushOverride_Dispose_IsIdempotent()
        {
            var key = UniqueKey();
            GameConfigState.Manager.Set(key, false);
            _sentCommands.Clear();

            var handle = GameConfigState.Manager.PushOverride(key, true);
            handle.Dispose();
            handle.Dispose();

            Assert.AreEqual(2, _sentCommands.Count); // override + one restore, not two restores
        }

        private static string UniqueKey() => "unittest-" + Guid.NewGuid().ToString("N");
    }
}
