using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Avalonia.Threading;
using TweakerCore.Skyboxes;

namespace TweakerUI.Core
{
    /// <summary>
    /// Application-wide sky catalog and state. Always subscribed: the plugin sends its full catalog once per link.
    /// All state and events belong to the UI thread; the transport only carries protocol lines.
    /// </summary>
    public static class SkyboxOverlayBridge
    {
        private static Func<string, bool> _send = OverlayHelper.SendSkybox;
        private static Func<bool> _connected = () => OverlayHelper.LinkState == PluginLinkState.Connected;
        private static readonly List<SkyCatalogEntry> _incoming = new List<SkyCatalogEntry>();
        private static bool _initialized;
        private static int _generation;

        public static TimeSpan ConfirmTimeout { get; set; } = TimeSpan.FromSeconds(5);

        public static bool IsLive { get; private set; }
        public static bool IsSynced { get; private set; }
        public static IReadOnlyList<SkyCatalogEntry> Catalog { get; private set; } = Array.Empty<SkyCatalogEntry>();
        public static SkyLiveState State { get; private set; }

        public static bool? PendingEnabled { get; private set; }
        public static SkySelection PendingSelection { get; private set; }
        public static bool IsPending => PendingEnabled != null || PendingSelection != null;

        public static event EventHandler Changed;
        public static event EventHandler RequestTimedOut;

        internal static void Initialize()
        {
            if (_initialized)
                return;

            _initialized = true;
            OverlayHelper.SkyboxMessageReceived += (_, line) => Dispatcher.UIThread.Post(() => Receive(line));
            OverlayHelper.LinkStateChanged += (_, _) => Dispatcher.UIThread.Post(RefreshLink);
            RefreshLink();
        }

        public static void UseTransport(Func<string, bool> send, Func<bool> connected)
        {
            _send = send ?? throw new ArgumentNullException(nameof(send));
            _connected = connected ?? throw new ArgumentNullException(nameof(connected));
            RefreshLink();
        }

        public static void RefreshLink()
        {
            var live = _connected();
            if (live == IsLive)
                return;

            IsLive = live;
            IsSynced = false;
            Catalog = Array.Empty<SkyCatalogEntry>();
            _incoming.Clear();
            State = null;
            ClearPending();
            Changed?.Invoke(null, EventArgs.Empty);

            // HANDSHAKE_ACK completes an asynchronous continuation in OverlayHelper. The first catalog can
            // already arrive before that continuation publishes Connected; ask again once the link is usable.
            if (live)
                Rescan();
        }

        public static void Receive(string line)
        {
            if (!IsLive)
                return;

            if (SkyWire.TryParseEntry(line, out var entry))
            {
                _incoming.Add(entry);
            }
            else if (SkyWire.TryParseSynced(line, out var count))
            {
                // Commit complete snapshots only. A truncated batch must not replace the last usable catalog.
                IsSynced = count == _incoming.Count;
                if (IsSynced)
                    Catalog = _incoming.ToArray();

                _incoming.Clear();
                Changed?.Invoke(null, EventArgs.Empty);
            }
            else if (SkyWire.TryParseState(line, out var state))
            {
                State = state;
                // Every STATE is authoritative, including failure, missing, or a selection made in the overlay.
                ClearPending();
                Changed?.Invoke(null, EventArgs.Empty);
            }
        }

        public static bool RequestEnable(bool enabled) => Request(SkyWire.Enable(enabled), enabled, null);

        public static bool RequestSelect(SkySelection sky) => Request(SkyWire.Select(sky), null, sky);

        public static void Rescan()
        {
            if (!IsLive)
                return;

            IsSynced = false;
            _incoming.Clear();
            _send(SkyWire.Rescan());
            Changed?.Invoke(null, EventArgs.Empty);
        }

        private static bool Request(string line, bool? enabled, SkySelection sky)
        {
            if (!IsLive)
                return false;

            PendingEnabled = enabled ?? PendingEnabled;
            PendingSelection = sky ?? PendingSelection;
            var generation = ++_generation;
            if (!_send(line))
            {
                ClearPending();
                Changed?.Invoke(null, EventArgs.Empty);
                return false;
            }

            Changed?.Invoke(null, EventArgs.Empty);
            _ = ExpireAsync(generation);
            return true;
        }

        private static void ClearPending()
        {
            PendingEnabled = null;
            PendingSelection = null;
            _generation++;
        }

        private static async Task ExpireAsync(int generation)
        {
            await Task.Delay(ConfirmTimeout).ConfigureAwait(false);
            Dispatcher.UIThread.Post(() =>
            {
                if (generation != _generation || !IsPending)
                    return;

                ClearPending();
                Logger.Log("Skyboxes", "the game did not confirm the sky change");
                Changed?.Invoke(null, EventArgs.Empty);
                RequestTimedOut?.Invoke(null, EventArgs.Empty);
            });
        }
    }
}
