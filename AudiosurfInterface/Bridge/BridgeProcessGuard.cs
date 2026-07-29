using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace AudiosurfInterface.Bridge
{
    /// <summary>
    /// One asbridge.exe per Tweaker, and no strays. A bridge instance this process did not spawn is
    /// useless to it - the pipe name is generated per connection (a fresh GUID, see
    /// AsBridgeConnection's constructor) and there is no way to ask a running bridge which pipe it
    /// happens to be listening on - so a leftover from a crashed session can only sit there holding
    /// the game window's registration hostage. Such instances get killed right before a new bridge is
    /// spawned.
    ///
    /// "Ours" is tracked as a PID set rather than inferred: AudiosurfHandle's reinitialize path
    /// deliberately starts the replacement connection before disposing the previous one, so at that
    /// moment two bridges owned by this very process are alive at once and neither may kill the other.
    /// </summary>
    public static class BridgeProcessGuard
    {
        /// <summary>
        /// Opt-in, set by the host that owns the application-wide single-instance guard (see
        /// TweakerUI's Program.Main). Killing every other asbridge.exe in the session is only
        /// defensible from a process that has already established it is the one legitimate Tweaker;
        /// anything else linking this assembly - the test host above all, which touches
        /// AudiosurfHandle.Instance and would otherwise shoot down the developer's actually running
        /// Tweaker - leaves other people's bridges alone.
        /// </summary>
        public static bool ExclusiveOwnership { get; set; }

        internal const string BridgeProcessName = "asbridge";

        private static readonly object _lock = new object();
        private static readonly HashSet<int> _ownedPids = new HashSet<int>();

        // Test seams. Everything real about this class needs live processes; the part worth testing is
        // which of a given set of bridges counts as foreign.
        internal static Func<IReadOnlyList<BridgeProcessInfo>> EnumerateBridges = EnumerateLiveBridges;
        internal static Action<int> KillBridge = KillLiveBridge;
        internal static Func<int> CurrentSessionId = GetCurrentSessionId;

        internal static void RegisterOwned(int pid)
        {
            lock (_lock)
                _ownedPids.Add(pid);
        }

        internal static void ForgetOwned(int pid)
        {
            lock (_lock)
                _ownedPids.Remove(pid);
        }

        internal static bool IsOwned(int pid)
        {
            lock (_lock)
                return _ownedPids.Contains(pid);
        }

        /// <summary>
        /// Kills every asbridge.exe in this logon session that this process did not spawn. Never
        /// throws: a bridge that exits on its own between enumeration and kill, or one this process
        /// has no rights over, is reported through <paramref name="report"/> and skipped.
        /// </summary>
        internal static int KillForeignBridges(Action<AsBridgeDiagnostic> report)
        {
            if (!ExclusiveOwnership)
                return 0;

            IReadOnlyList<BridgeProcessInfo> bridges;
            int sessionId;

            try
            {
                bridges = EnumerateBridges();
                sessionId = CurrentSessionId();
            }
            catch (Exception ex)
            {
                report?.Invoke(new AsBridgeDiagnostic(AsBridgeDiagnosticLevel.Warning, "BridgeGuard",
                    $"could not enumerate running {BridgeProcessName}.exe instances: {ex.Message}", ex));
                return 0;
            }

            var killed = 0;
            foreach (var pid in SelectForeign(bridges, sessionId, IsOwned))
            {
                try
                {
                    KillBridge(pid);
                    killed++;
                    report?.Invoke(new AsBridgeDiagnostic(AsBridgeDiagnosticLevel.Warning, "BridgeGuard",
                        $"killed orphaned {BridgeProcessName}.exe (pid {pid}) - its pipe name is unknowable, so it could never be reused"));
                }
                catch (Exception ex)
                {
                    report?.Invoke(new AsBridgeDiagnostic(AsBridgeDiagnosticLevel.Warning, "BridgeGuard",
                        $"could not kill orphaned {BridgeProcessName}.exe (pid {pid}): {ex.Message}", ex));
                }
            }

            return killed;
        }

        /// <summary>
        /// Pure selection half of <see cref="KillForeignBridges"/>: a bridge is foreign when it runs in
        /// this logon session and was not spawned by this process. Other sessions are left alone -
        /// that would be another user's tweaker, and terminating it would fail with access denied
        /// anyway.
        /// </summary>
        internal static IEnumerable<int> SelectForeign(
            IReadOnlyList<BridgeProcessInfo> bridges, int sessionId, Func<int, bool> isOwned)
        {
            if (bridges == null)
                yield break;

            foreach (var bridge in bridges)
            {
                if (bridge.SessionId != sessionId)
                    continue;
                if (isOwned(bridge.Pid))
                    continue;

                yield return bridge.Pid;
            }
        }

        private static IReadOnlyList<BridgeProcessInfo> EnumerateLiveBridges()
        {
            var result = new List<BridgeProcessInfo>();

            foreach (var process in Process.GetProcessesByName(BridgeProcessName))
            {
                using (process)
                {
                    try
                    {
                        result.Add(new BridgeProcessInfo(process.Id, process.SessionId));
                    }
                    catch (InvalidOperationException)
                    {
                        // Exited between the snapshot and the property read - nothing left to kill.
                    }
                }
            }

            return result;
        }

        private static void KillLiveBridge(int pid)
        {
            using var process = Process.GetProcessById(pid);
            process.Kill();
            process.WaitForExit(KillWaitMs);
        }

        private static int GetCurrentSessionId()
        {
            using var self = Process.GetCurrentProcess();
            return self.SessionId;
        }

        private const int KillWaitMs = 2000;
    }

    internal readonly struct BridgeProcessInfo
    {
        public BridgeProcessInfo(int pid, int sessionId)
        {
            Pid = pid;
            SessionId = sessionId;
        }

        public int Pid { get; }
        public int SessionId { get; }
    }
}
