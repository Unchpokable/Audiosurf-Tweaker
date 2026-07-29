namespace AudiosurfInterface.Tests
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using AudiosurfInterface.Bridge;
    using NUnit.Framework;

    // The guard's real work needs live processes; what is testable - and what actually decides whether
    // the developer's running Tweaker survives a test run - is which bridges it picks and which it
    // refuses to touch. Everything below drives it through its seams (EnumerateBridges/KillBridge/
    // CurrentSessionId), which is why those exist.
    [TestFixture]
    public class BridgeProcessGuardTests
    {
        private const int OurSession = 1;

        private readonly List<int> _registered = new List<int>();
        private List<int> _killed;

        private Func<IReadOnlyList<BridgeProcessInfo>> _realEnumerate;
        private Action<int> _realKill;
        private Func<int> _realSessionId;

        [SetUp]
        public void SetUp()
        {
            _killed = new List<int>();

            _realEnumerate = BridgeProcessGuard.EnumerateBridges;
            _realKill = BridgeProcessGuard.KillBridge;
            _realSessionId = BridgeProcessGuard.CurrentSessionId;

            BridgeProcessGuard.ExclusiveOwnership = true;
            BridgeProcessGuard.CurrentSessionId = () => OurSession;
            BridgeProcessGuard.KillBridge = pid => _killed.Add(pid);
            BridgeProcessGuard.EnumerateBridges = () => new List<BridgeProcessInfo>();
        }

        [TearDown]
        public void TearDown()
        {
            foreach (var pid in _registered)
                BridgeProcessGuard.ForgetOwned(pid);
            _registered.Clear();

            // Static state, and the seams default to the real process APIs - a fixture that left them
            // pointing at its own lambdas would have every later fixture killing nothing (or worse).
            BridgeProcessGuard.ExclusiveOwnership = false;
            BridgeProcessGuard.EnumerateBridges = _realEnumerate;
            BridgeProcessGuard.KillBridge = _realKill;
            BridgeProcessGuard.CurrentSessionId = _realSessionId;
        }

        private void Own(int pid)
        {
            BridgeProcessGuard.RegisterOwned(pid);
            _registered.Add(pid);
        }

        private static IReadOnlyList<BridgeProcessInfo> Bridges(params (int Pid, int Session)[] entries)
        {
            return entries.Select(e => new BridgeProcessInfo(e.Pid, e.Session)).ToList();
        }

        [Test]
        public void SelectForeign_SkipsBridgesThisProcessSpawned()
        {
            var foreign = BridgeProcessGuard
                .SelectForeign(Bridges((100, OurSession), (200, OurSession)), OurSession, pid => pid == 100)
                .ToList();

            Assert.That(foreign, Is.EqualTo(new[] { 200 }));
        }

        [Test]
        public void SelectForeign_SkipsOtherLogonSessions()
        {
            // Another user's tweaker: not ours to kill, and terminating it would fail anyway.
            var foreign = BridgeProcessGuard
                .SelectForeign(Bridges((100, OurSession), (200, OurSession + 1)), OurSession, _ => false)
                .ToList();

            Assert.That(foreign, Is.EqualTo(new[] { 100 }));
        }

        [Test]
        public void SelectForeign_OnEmptyOrNullInput_YieldsNothing()
        {
            Assert.That(BridgeProcessGuard.SelectForeign(Bridges(), OurSession, _ => false), Is.Empty);
            Assert.That(BridgeProcessGuard.SelectForeign(null, OurSession, _ => false), Is.Empty);
        }

        [Test]
        public void KillForeignBridges_KillsStraysAndSparesOwnedOnes()
        {
            Own(100);
            BridgeProcessGuard.EnumerateBridges = () => Bridges((100, OurSession), (200, OurSession), (300, OurSession));

            var killed = BridgeProcessGuard.KillForeignBridges(null);

            Assert.That(killed, Is.EqualTo(2));
            Assert.That(_killed, Is.EquivalentTo(new[] { 200, 300 }));
        }

        // The reinitialize path (AudiosurfHandle.ReinitializeConnectionLocked) deliberately starts the
        // replacement bridge before disposing the previous one, so two bridges owned by this process
        // are briefly alive at once and neither may kill the other.
        [Test]
        public void KillForeignBridges_DuringAReinitializeOverlap_SparesBothOwnedBridges()
        {
            Own(100);
            Own(101);
            BridgeProcessGuard.EnumerateBridges = () => Bridges((100, OurSession), (101, OurSession));

            Assert.That(BridgeProcessGuard.KillForeignBridges(null), Is.Zero);
            Assert.That(_killed, Is.Empty);
        }

        [Test]
        public void KillForeignBridges_WithoutExclusiveOwnership_TouchesNothing()
        {
            // The state a test host or any other consumer of this assembly runs in.
            BridgeProcessGuard.ExclusiveOwnership = false;
            BridgeProcessGuard.EnumerateBridges = () => Bridges((200, OurSession));

            Assert.That(BridgeProcessGuard.KillForeignBridges(null), Is.Zero);
            Assert.That(_killed, Is.Empty);
        }

        [Test]
        public void KillForeignBridges_WhenEnumerationThrows_ReportsAndKeepsGoing()
        {
            var diagnostics = new List<AsBridgeDiagnostic>();
            BridgeProcessGuard.EnumerateBridges = () => throw new InvalidOperationException("snapshot failed");

            var killed = BridgeProcessGuard.KillForeignBridges(diagnostics.Add);

            Assert.That(killed, Is.Zero);
            Assert.That(diagnostics, Has.Count.EqualTo(1));
            Assert.That(diagnostics[0].Level, Is.EqualTo(AsBridgeDiagnosticLevel.Warning));
        }

        [Test]
        public void KillForeignBridges_WhenOneKillFails_StillKillsTheRest()
        {
            var diagnostics = new List<AsBridgeDiagnostic>();
            BridgeProcessGuard.EnumerateBridges = () => Bridges((200, OurSession), (300, OurSession));
            BridgeProcessGuard.KillBridge = pid =>
            {
                if (pid == 200)
                    throw new InvalidOperationException("already exited");
                _killed.Add(pid);
            };

            var killed = BridgeProcessGuard.KillForeignBridges(diagnostics.Add);

            Assert.That(killed, Is.EqualTo(1));
            Assert.That(_killed, Is.EqualTo(new[] { 300 }));
            Assert.That(diagnostics, Has.Count.EqualTo(2)); // one failure, one success
        }

        [Test]
        public void ForgetOwned_ReleasesThePidForFutureKills()
        {
            // PIDs get recycled: a stale entry would spare a genuinely foreign bridge that later
            // reused the number.
            Own(100);
            Assert.That(BridgeProcessGuard.IsOwned(100), Is.True);

            BridgeProcessGuard.ForgetOwned(100);

            Assert.That(BridgeProcessGuard.IsOwned(100), Is.False);
        }
    }
}
