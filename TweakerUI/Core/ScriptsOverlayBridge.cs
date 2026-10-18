using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Avalonia.Threading;
using TweakerCore.Scripting;

namespace TweakerUI.Core
{
    /// <summary>
    /// The host half of the scripts protocol (Docs/Internal/overlay-scripts.md, script-manager.md §7): what
    /// the running plugin says each script is doing, its run order, the switches and the drag waiting for its
    /// confirmation, and the commands that go the other way. OverlayHelper only carries the lines; ScriptWire spells them.
    ///
    /// One per application and static, like OverlayHelper, because two tabs use it - Scripts switches and shows
    /// state, Script Dev asks for a reload after installing - and the state has to be there whichever of them is on
    /// screen: the plugin sends the whole list once, after the handshake, and only changes after that.
    ///
    /// Everything here lives on the UI thread. Lines from the plugin arrive on the bridge's thread and are posted.
    ///
    /// The transport can be swapped (<see cref="UseTransport"/>) and lines fed in (<see cref="Receive"/>), which is
    /// how Tools/UiStand drives the Scripts tab against a fake plugin.
    /// </summary>
    public static class ScriptsOverlayBridge
    {
        private sealed class Pending
        {
            public bool Desired;
            public int Generation;
        }

        private sealed class PendingOrder
        {
            public IReadOnlyList<string> Files;
            public int Generation;
        }

        private static readonly Dictionary<string, ScriptLiveState> _states = new Dictionary<string, ScriptLiveState>(StringComparer.Ordinal);
        private static readonly Dictionary<string, Pending> _pending = new Dictionary<string, Pending>(StringComparer.Ordinal);

        // The run order the plugin last sent (SCRIPT_ORDER), and a drag still waiting for it to confirm.
        private static IReadOnlyList<string> _order;
        private static PendingOrder _pendingOrder;

        private static Func<string, bool> _send = OverlayHelper.SendScripts;
        private static Func<bool> _connected = () => OverlayHelper.LinkState == PluginLinkState.Connected;

        private static bool _initialized;
        private static int _generation;

        /// <summary>
        /// How long a switch waits for the plugin's SCRIPT_STATE before it springs back - the same 5 s the overlay
        /// gives the host for NOTIFY_TWEAK, the other way round.
        /// </summary>
        public static TimeSpan ConfirmTimeout { get; set; } = TimeSpan.FromSeconds(5);

        /// <summary>Connected to the plugin: switches go to it, and the states below are its word.</summary>
        public static bool IsLive { get; private set; }

        /// <summary>The whole list has arrived since the link came up (SCRIPT_SYNCED).</summary>
        public static bool IsSynced { get; private set; }

        /// <summary>Anything changed - a state, a pending switch, the link. UI thread.</summary>
        public static event EventHandler Changed;

        /// <summary>A switch went unconfirmed and was dropped; the argument is the file. UI thread.</summary>
        public static event EventHandler<string> RequestTimedOut;

        /// <summary>A new order went unconfirmed and was dropped; the rows go back to the plugin's. UI thread.</summary>
        public static event EventHandler OrderTimedOut;

        /// <summary>
        /// The run order to show (Docs/Internal/script-coexistence.md §3): a drag waiting for confirmation,
        /// else what the plugin last said; null when it has said nothing. File names, top first, including scripts
        /// whose files went away during the session.
        /// </summary>
        public static IReadOnlyList<string> Order => _pendingOrder?.Files ?? _order;

        /// <summary>A drag was sent and the plugin has not answered yet.</summary>
        public static bool IsOrderPending => _pendingOrder != null;

        internal static void Initialize()
        {
            if (_initialized)
                return;
            _initialized = true;

            OverlayHelper.ScriptsMessageReceived += (_, line) => Dispatcher.UIThread.Post(() => Receive(line));
            OverlayHelper.LinkStateChanged += (_, _) => Dispatcher.UIThread.Post(RefreshLink);
            RefreshLink();
        }

        /// <summary>Replaces the link to the plugin - for the UI stand. Re-reads whether it is connected.</summary>
        public static void UseTransport(Func<string, bool> send, Func<bool> connected)
        {
            _send = send ?? throw new ArgumentNullException(nameof(send));
            _connected = connected ?? throw new ArgumentNullException(nameof(connected));
            RefreshLink();
        }

        /// <summary>Re-reads whether the plugin is connected. Losing it forgets everything it said and every switch waiting for it.</summary>
        public static void RefreshLink()
        {
            var live = _connected();
            if (live == IsLive)
                return;

            IsLive = live;
            IsSynced = false;
            _states.Clear();
            _pending.Clear();
            _order = null;
            _pendingOrder = null;
            _generation++;
            Changed?.Invoke(null, EventArgs.Empty);
        }

        /// <summary>One line from the plugin. Lines this build does not understand are ignored, like every other op.</summary>
        public static void Receive(string line)
        {
            if (ScriptWire.TryParseState(line, out var state))
            {
                _states[state.FileName] = state;

                // Any state for the file is the answer, whatever it says: the plugin answers every SET, and a "failed"
                // or "missing" is exactly what the switch should now show.
                _pending.Remove(state.FileName);
                Changed?.Invoke(null, EventArgs.Empty);
                return;
            }

            // Any order is the answer to a drag, like any state is to a switch: the plugin answers every SCRIPT_ORDER
            // with the order it now has, and that - not what was asked - is what the rows should show.
            if (ScriptWire.TryParseOrder(line, out var order))
            {
                _order = order;
                _pendingOrder = null;
                Changed?.Invoke(null, EventArgs.Empty);
                return;
            }

            if (ScriptWire.TryParseSynced(line, out _))
            {
                IsSynced = true;
                Changed?.Invoke(null, EventArgs.Empty);
            }
        }

        /// <summary>
        /// Asks the plugin to run the scripts in this order - the whole order, top first. Shown at once and waits for
        /// the plugin's SCRIPT_ORDER, which also writes scripts.cfg; without an answer within
        /// <see cref="ConfirmTimeout"/> the rows go back. A second drag while one waits replaces it. Returns false
        /// when the plugin could not be asked at all.
        /// </summary>
        public static bool RequestOrder(IReadOnlyList<string> fileNames)
        {
            if (!IsLive || !_send(ScriptWire.Order(fileNames)))
                return false;

            var pending = new PendingOrder { Files = fileNames, Generation = ++_generation };
            _pendingOrder = pending;
            Changed?.Invoke(null, EventArgs.Empty);
            _ = ExpireOrderAsync(pending.Generation);
            return true;
        }

        /// <summary>What the plugin last said about a file, or null.</summary>
        public static ScriptLiveState StateOf(string fileName) =>
            fileName != null && _states.TryGetValue(fileName, out var state) ? state : null;

        /// <summary>The position a switch was moved to and is waiting to have confirmed, or null.</summary>
        public static bool? PendingOf(string fileName) =>
            fileName != null && _pending.TryGetValue(fileName, out var pending) ? pending.Desired : null;

        /// <summary>
        /// Asks the plugin to switch a script. Returns false when it could not be asked at all (no link). A second
        /// request for the same file while one waits replaces it: the last click wins, and the clock restarts.
        /// </summary>
        public static bool RequestSet(string fileName, bool enabled)
        {
            if (!IsLive || !_send(ScriptWire.Set(fileName, enabled)))
                return false;

            var pending = new Pending { Desired = enabled, Generation = ++_generation };
            _pending[fileName] = pending;
            Changed?.Invoke(null, EventArgs.Empty);
            _ = ExpireAsync(fileName, pending.Generation);
            return true;
        }

        /// <summary>
        /// Asks the plugin to read these files again - after installing or restoring them. One it has not seen yet
        /// makes it rescan the folder first. Fire and forget: the answer is a state like any other.
        /// </summary>
        public static void Reload(IEnumerable<string> fileNames)
        {
            if (!IsLive)
                return;

            foreach (var fileName in fileNames)
            {
                if (!string.IsNullOrEmpty(fileName))
                    _send(ScriptWire.Reload(fileName));
            }
        }

        /// <summary>Asks the plugin to look at the Scripts folder again. Answered with the whole list.</summary>
        public static void Rescan()
        {
            if (!IsLive)
                return;

            IsSynced = false;
            _send(ScriptWire.Rescan());
        }

        // Task.Delay rather than a DispatcherTimer: the timer's clock does not run in the headless UI stand, and this
        // is one of the things the stand checks.
        private static async Task ExpireAsync(string fileName, int generation)
        {
            await Task.Delay(ConfirmTimeout).ConfigureAwait(false);
            Dispatcher.UIThread.Post(() =>
            {
                if (!_pending.TryGetValue(fileName, out var pending) || pending.Generation != generation)
                    return;

                _pending.Remove(fileName);
                Logger.Log("Scripts", $"the game did not confirm switching {fileName} within {ConfirmTimeout.TotalSeconds:F0} s");
                Changed?.Invoke(null, EventArgs.Empty);
                RequestTimedOut?.Invoke(null, fileName);
            });
        }

        private static async Task ExpireOrderAsync(int generation)
        {
            await Task.Delay(ConfirmTimeout).ConfigureAwait(false);
            Dispatcher.UIThread.Post(() =>
            {
                if (_pendingOrder == null || _pendingOrder.Generation != generation)
                    return;

                _pendingOrder = null;
                Logger.Log("Scripts", $"the game did not confirm the new script order within {ConfirmTimeout.TotalSeconds:F0} s");
                Changed?.Invoke(null, EventArgs.Empty);
                OrderTimedOut?.Invoke(null, EventArgs.Empty);
            });
        }
    }
}
