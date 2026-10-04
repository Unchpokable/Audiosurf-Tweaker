using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Avalonia.Threading;
using TweakerCore.Scripting;

namespace TweakerUI.Core
{
    /// <summary>
    /// The host half of the scripts protocol (Docs/Internal/overlay-scripts.md, script-manager-roadmap.md §9): what
    /// the running plugin says each script is doing, the switches waiting for its confirmation, and the commands
    /// that go the other way. OverlayHelper only carries the lines; ScriptWire spells them.
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

        private static readonly Dictionary<string, ScriptLiveState> _states = new Dictionary<string, ScriptLiveState>(StringComparer.Ordinal);
        private static readonly Dictionary<string, Pending> _pending = new Dictionary<string, Pending>(StringComparer.Ordinal);

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

            if (ScriptWire.TryParseSynced(line, out _))
            {
                IsSynced = true;
                Changed?.Invoke(null, EventArgs.Empty);
            }
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
    }
}
