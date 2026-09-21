using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using AudiosurfInterface;
using Avalonia.Threading;
using Microsoft.Win32.SafeHandles;

namespace TweakerUI.Core
{
    /// <summary>
    /// What the host and an already-running TweakerPlugin have to do with each other
    /// (Docs/Internal/overlay-protocol.md, plugin-offline-mode.md §6.5): find out whether the plugin is in the
    /// game process at all, wait for it to say it is ready, perform the L3 handshake over OVERLAY_SEND, push
    /// the initial snapshot, and stay open for live updates.
    ///
    /// Injection is no longer how the plugin gets in - the game loads it from engine\channels\ by itself
    /// (§4.2), so by the time the host sees the game it is either already there or it is not. InjectHelper
    /// survives as the one-shot answer to "installed, but this session of the game started before it was"
    /// (Р-5), offered to the user rather than done behind their back.
    ///
    /// TweakerCore intentionally doesn't know about AudiosurfInterface (see overview.md), so this lives here
    /// rather than there.
    /// </summary>
    internal static class OverlayHelper
    {
        private const string HandshakeAckPayload = "HANDSHAKE_ACK";
        private const string HostDisconnectPayload = "HOST_DISCONNECT";
        private const string NotifyTweakPrefix = "NOTIFY_TWEAK ";
        private const string NotifySkinPrefix = "NOTIFY_SKIN ";

        // Quick Player owns a whole family of ops (Docs/Internal/overlay-quickplayer.md). Only the
        // prefix is known here - the grammar itself lives in QuickPlayerOverlayBridge, the same way
        // the plugin hands QP_* straight to its own module instead of parsing it in ipc/overlay_ipc.
        private const string QuickPlayerNotifyPrefix = "QP_NOTIFY_";

        /// <summary>
        /// The L3 grammar this build speaks. The plugin sends its own in HANDSHAKE_ACK; a mismatch is a failed
        /// handshake, not a degraded one - both sides would otherwise keep talking past each other in a protocol
        /// neither fully implements. Plugin side: k_protocol_version in src/ipc/overlay_ipc.cxx.
        /// </summary>
        private const int ExpectedProtocolVersion = 1;

        // Must match TweakerPlugin's presence::claim() (src/plugin/presence.cxx) exactly, including the
        // deliberate absence of any version or path component - the point is that a newer host can still
        // recognise an older, possibly renamed plugin sitting in the game.
        private const string OverlayInstanceMutexPrefix = @"Local\AudiosurfTweaker.Overlay.";
        private const string ReadyEventSuffix = ".Ready";

        private static readonly TimeSpan HandshakeTimeout = TimeSpan.FromSeconds(5);
        private const int HandshakeAttempts = 3;

        /// <summary>
        /// How long the plugin gets between "I am loaded" (the mutex) and "I am ready to be talked to" (the
        /// event it sets once the first D3D9 device is bound). Generous on purpose: loaded from channels\ the
        /// plugin is in the process seconds before the game has a device, and on a cold start with the game
        /// still reading its own assets off a spinning disk that gap is the game's, not the plugin's.
        /// </summary>
        private static readonly TimeSpan ReadyTimeout = TimeSpan.FromSeconds(60);

        private static readonly object _lock = new object();

        private static bool _initialized;
        private static bool _ready;
        private static TaskCompletionSource<HandshakeAck> _handshakeAckTcs;
        private static PluginLinkState _linkState = PluginLinkState.Idle;
        private static string _connectedVersion;
        private static string _connectedLoadMode;

        /// <summary>The game PID the "this session started without the plugin" offer was already made for (§6.5).</summary>
        private static int _offeredForPid;

        /// <summary>
        /// Fired once the L3 handshake is acked and the channel is ready for live pushes.
        /// OverlayHelper pushes the GameConfigState tweak snapshot itself; other domains such as
        /// SkinChangerViewModel subscribe to push their own initial state.
        /// </summary>
        internal static event EventHandler OverlayReady;

        /// <summary>The link state changed - the Settings tab's plugin status line (§6.6) is built from it.</summary>
        internal static event EventHandler LinkStateChanged;

        /// <summary>
        /// Reverse-sync (Docs/Internal/overlay-protocol.md, "Reverse-sync: NOTIFY_TWEAK/NOTIFY_SKIN"):
        /// the user clicked a tweak/skin directly in the in-game overlay. TweakerViewModel/
        /// SkinChangerViewModel subscribe and apply the request through their normal state paths;
        /// GameConfigState.StateChanged and PushCurrentSkin provide the confirming echo.
        /// OverlayHelper deliberately doesn't call into the ViewModels itself (would invert the
        /// dependency direction every other OverlayHelper touchpoint uses - see OverlayReady above).
        /// </summary>
        internal static event EventHandler<TweakRequestedEventArgs> TweakRequested;
        internal static event EventHandler<string> SkinRequested;

        /// <summary>
        /// A QP_NOTIFY_* op arrived from the overlay's Quick Player tab, forwarded verbatim (op line
        /// included). Unlike the two above this one is not pre-parsed: Quick Player has a dozen ops
        /// with varying arity, and splitting their grammar across this class and its consumer is how
        /// the two halves would drift apart.
        /// </summary>
        internal static event EventHandler<string> QuickPlayerRequested;

        internal static PluginLinkState LinkState
        {
            get { lock (_lock) return _linkState; }
        }

        /// <summary>Version the connected plugin reported in its ack, or null.</summary>
        internal static string ConnectedVersion
        {
            get { lock (_lock) return _connectedVersion; }
        }

        /// <summary>"channels" or "injected" - how the plugin got into the game (§5.2).</summary>
        internal static string ConnectedLoadMode
        {
            get { lock (_lock) return _connectedLoadMode; }
        }

        internal static void Initialize()
        {
            if (_initialized)
                return;
            _initialized = true;

            AudiosurfHandle.Instance.Registered += OnRegistered;
            AudiosurfHandle.Instance.StateChanged += OnAudiosurfStateChanged;
            AudiosurfHandle.Instance.OverlayMessageReceived += OnOverlayMessageReceived;
            GameConfigState.Manager.StateChanged += OnGameConfigStateChanged;
        }

        /// <summary>
        /// The game went away (window lost, or the bridge connection dropped). Nothing is reachable through a
        /// dead process, and a status line still reading "Connected v0.1.0" after the game closed is worse than
        /// no status line at all.
        /// </summary>
        private static void OnAudiosurfStateChanged(object sender, EventArgs e)
        {
            if (AudiosurfHandle.Instance.IsValid)
                return;

            lock (_lock)
                _ready = false;

            SetLinkState(PluginLinkState.Idle);
        }

        /// <summary>
        /// Tells the plugin the host is going away, so the overlay drops to its offline state at once instead of
        /// waiting for its own watchdog to notice the bridge window is gone (§5.1). Best-effort by design:
        /// OverlayCommand hands the line to the bridge synchronously, and the bridge is still alive at this
        /// point - if it is not, the ~1 s watchdog covers it.
        ///
        /// Two callers, and the second is the reason this does not simply assume it is connected: the app
        /// shutting down, and the user turning syncing off. A setting that only takes effect the next time the
        /// game registers is a setting that appears not to work.
        /// </summary>
        internal static void Disconnect()
        {
            bool wasReady;
            lock (_lock)
            {
                wasReady = _ready;
                _ready = false;
            }

            try
            {
                if (wasReady)
                    AudiosurfHandle.Instance.OverlayCommand(HostDisconnectPayload);
            }
            catch (Exception ex)
            {
                // Shutdown path: the bridge may already be tearing down. Nothing to recover, and throwing here
                // would take the rest of the application teardown with it.
                Logger.Log("OverlayHelper", $"Could not send HOST_DISCONNECT: {ex.Message}");
            }

            SetLinkState(PluginLinkState.Idle);
        }

        private static void PushTweak(GameConfigSnapshot snapshot)
        {
            if (!IsReady)
                return;

            var definition = GameTweakCatalog.FindByConfigKey(snapshot.Key);
            if (definition == null)
                return;

            var enabled = definition.ToEnabledValue(snapshot.EffectiveValue);
            var source = snapshot.OverrideSource == GameConfigOverrideSource.QuickPlayer ? "quick_player" : "global";
            AudiosurfHandle.Instance.OverlayCommand(
                $"TWEAK_SET {definition.WireName} {(enabled ? "true" : "false")} {source}");
        }

        private static void PushTweakSnapshot()
        {
            foreach (var snapshot in GameConfigState.Manager.GetKnownTweakSnapshots())
                PushTweak(snapshot);
        }

        internal static void PushCurrentSkin(string name)
        {
            if (!IsReady)
                return;

            AudiosurfHandle.Instance.OverlayCommand($"CURRENT_SKIN {Uri.EscapeDataString(name ?? string.Empty)}");
        }

        internal static void PushSkinList(IEnumerable<string> names)
        {
            if (!IsReady)
                return;

            var payload = string.Join(" ", names.Select(Uri.EscapeDataString));
            AudiosurfHandle.Instance.OverlayCommand(
                string.IsNullOrEmpty(payload) ? "SKIN_LIST" : $"SKIN_LIST {payload}");
        }

        /// <summary>
        /// Sends one already-formatted Quick Player op line (e.g. "QP_STOPPED"). Deliberately a single
        /// passthrough rather than a typed method per op: this class is the transport half of TW_OVL,
        /// and every QP_* op it learned to spell would be a second place to keep the format in sync.
        /// </summary>
        internal static void SendQuickPlayer(string opLine)
        {
            if (!IsReady || string.IsNullOrEmpty(opLine))
                return;

            AudiosurfHandle.Instance.OverlayCommand(opLine);
        }

        private static bool IsReady
        {
            get { lock (_lock) return _ready; }
        }

        private static void OnRegistered(object sender, EventArgs e)
        {
            if (!SettingsProvider.SyncOverlayWithTweaker)
            {
                SetLinkState(PluginLinkState.Idle);
                return;
            }

            _ = ConnectAsync();
        }

        /// <summary>
        /// Runs the connection flow again against the game that is already registered. Registered fires once
        /// per game session, so without this every answer taken from it - is the plugin installed, is syncing
        /// on - is frozen at the moment the game connected, and anything the user changes afterwards has no
        /// effect until they restart something. The two things that change it are installing the plugin
        /// (§6.4: files first, then the offer to load it into the session already running) and turning syncing
        /// back on.
        /// </summary>
        internal static void Reevaluate()
        {
            if (!SettingsProvider.SyncOverlayWithTweaker || !AudiosurfHandle.Instance.IsValid)
                return;

            _ = ConnectAsync();
        }

        /// <summary>
        /// Drops the "already asked this game session" guard. Taking the plugin out answers the question the
        /// offer was asking, so installing it again is a new question - without this, the second install in one
        /// session would put the files in place and then say nothing, which is the failure this guard is
        /// supposed to prevent, not cause.
        /// </summary>
        internal static void ForgetLoadOffer() => Interlocked.Exchange(ref _offeredForPid, 0);

        /// <summary>
        /// The whole of §6.5's flow chart. Registered can fire more than once per game session (a bridge reset,
        /// a re-registration), so every branch has to be safe to re-enter against a plugin that is already
        /// connected - which it is: the handshake is also the adoption mechanism, re-pointing the plugin at
        /// whatever bridge window this host currently owns.
        /// </summary>
        private static async Task ConnectAsync()
        {
            lock (_lock)
                _ready = false;

            var pid = AudiosurfHandle.Instance.GamePID;
            if (pid == 0)
                return;

            if (!IsPluginInProcess(pid, out var detail))
            {
                // Nothing of ours in the game. Either the plugin is not installed - nothing to do and nothing to
                // say - or it is, and this session of the game simply started before it was put there.
                if (!PluginService.IsInstalled)
                {
                    SetLinkState(PluginLinkState.Idle);
                    return;
                }

                SetLinkState(PluginLinkState.NotLoadedInSession);
                await OfferToLoadIntoRunningGameAsync(pid);
                return;
            }

            SetLinkState(PluginLinkState.Waiting);

            // Loaded from channels\, the plugin is in the process long before the game has a D3D9 device, and
            // TW_OVL cannot be received until it does (there is no window to receive it on yet). Handshaking
            // into that gap is how a perfectly healthy plugin gets declared stale.
            if (!await WaitForPluginReadyAsync(pid, ReadyTimeout))
            {
                await SuspendForStalePluginAsync($"{detail}, and it never reported itself ready");
                return;
            }

            switch (await BeginHandshakeAsync(HandshakeTimeout, HandshakeAttempts))
            {
                case HandshakeOutcome.Connected:
                    return;

                case HandshakeOutcome.ProtocolMismatch:
                    // It answered, so it is neither stale nor hung - it is the wrong build. Stopping the service
                    // over that would be punishing the user for a version skew they can fix in one click.
                    Dispatcher.UIThread.Post(() => ApplicationNotificationManager.Manager.ShowWarning("Plugin version mismatch",
                        "The TweakerPlugin loaded in Audiosurf speaks a different overlay protocol than this Tweaker. " +
                        "Update the plugin from Settings, then restart the game."));
                    return;

                default:
                    await SuspendForStalePluginAsync(detail);
                    return;
            }
        }

        /// <summary>
        /// Whether a plugin instance holds its single-instance mutex in that process. This proves presence and
        /// never absence - which is all the flow needs it to do now that the host no longer injects on its own:
        /// a plugin this misses is one the user will be offered an injection for, and they can say no.
        /// </summary>
        private static bool IsPluginInProcess(int pid, out string detail)
        {
            detail = null;

            var handle = OpenMutex(SYNCHRONIZE, false, OverlayInstanceMutexPrefix + pid.ToString(CultureInfo.InvariantCulture));
            if (handle != IntPtr.Zero)
            {
                CloseHandle(handle);
                detail = "its single-instance mutex is held in the game process";
                return true;
            }

            // Distinguishing the two failures matters: "no such object" is a genuine absence, while
            // "access denied" means the object exists and belongs to a context this process cannot
            // touch. Treating the latter as absence would offer to inject a second copy on top of a live one.
            if (Marshal.GetLastWin32Error() == ERROR_ACCESS_DENIED)
            {
                detail = "its single-instance mutex exists but is not accessible from this process";
                return true;
            }

            return false;
        }

        /// <summary>
        /// Waits on the plugin's manual-reset ready event (§5.1). The event is created right after the mutex, so
        /// a caller that just saw the mutex may still be a few instructions early - hence the short open retry
        /// rather than treating "not there yet" as failure.
        /// </summary>
        private static async Task<bool> WaitForPluginReadyAsync(int pid, TimeSpan timeout)
        {
            var name = OverlayInstanceMutexPrefix + pid.ToString(CultureInfo.InvariantCulture) + ReadyEventSuffix;
            var deadline = DateTime.UtcNow + timeout;

            // OpenEventW rather than EventWaitHandle.OpenExisting: the framework helper asks for EVENT_MODIFY_STATE
            // as well, and the only thing wanted here is the right to wait. Asking for the power to signal the
            // plugin's own ready event would be both untrue and one more way to be refused.
            IntPtr raw;
            while ((raw = OpenEvent(SYNCHRONIZE, false, name)) == IntPtr.Zero)
            {
                if (Marshal.GetLastWin32Error() == ERROR_ACCESS_DENIED)
                {
                    // It is there but out of reach - nothing more to learn, and the handshake below is the real
                    // test anyway.
                    Logger.Log("OverlayHelper", $"Plugin ready event '{name}' is not accessible - handshaking anyway.");
                    return true;
                }

                if (DateTime.UtcNow >= deadline)
                {
                    Logger.Log("OverlayHelper", $"Plugin ready event '{name}' never appeared.");
                    return false;
                }

                await Task.Delay(200);
            }

            using (var handle = new EventWaitHandle(false, EventResetMode.ManualReset) { SafeWaitHandle = new SafeWaitHandle(raw, ownsHandle: true) })
            {
                var remaining = deadline - DateTime.UtcNow;
                if (remaining < TimeSpan.Zero)
                    remaining = TimeSpan.Zero;

                return await WaitOneAsync(handle, remaining);
            }
        }

        /// <summary>
        /// WaitHandle.WaitOne without parking a thread pool thread for up to a minute - the wait is handed to the
        /// pool's own wait machinery, which is what it is there for.
        /// </summary>
        private static Task<bool> WaitOneAsync(WaitHandle handle, TimeSpan timeout)
        {
            var tcs = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
            RegisteredWaitHandle registration = null;

            registration = ThreadPool.RegisterWaitForSingleObject(
                handle,
                (_, timedOut) =>
                {
                    registration?.Unregister(null);
                    tcs.TrySetResult(!timedOut);
                },
                null,
                timeout,
                executeOnlyOnce: true);

            return tcs.Task;
        }

        /// <summary>
        /// The game was already running when the plugin was installed (or the user started it without one and
        /// installed it since). Offered once per game PID: a bridge reset re-raises Registered, and being asked
        /// the same question on every one of those is how a dialog stops being read.
        /// </summary>
        private static async Task OfferToLoadIntoRunningGameAsync(int pid)
        {
            if (Interlocked.Exchange(ref _offeredForPid, pid) == pid)
                return;

            var pluginPath = PluginService.InstalledPluginPath;
            var injectorPath = PluginService.InjectorPath;
            var haveInjector = File.Exists(injectorPath);

            var message = "The Tweaker plugin is installed, but this session of Audiosurf was started without it. "
                          + (haveInjector
                              ? "It can be loaded into the running game now, or the game can be restarted so it loads the plugin by itself."
                              : "InjectHelper.exe is missing - most likely your antivirus removed it - so it cannot be loaded into the running game. "
                                + "Restart the game and it will load the plugin by itself. To get InjectHelper.exe back, download Tweaker again from "
                                + "https://github.com/Unchpokable/Audiosurf-Tweaker/releases and add the Tweaker folder to your antivirus exclusions.");

            var choice = await Dispatcher.UIThread.InvokeAsync(() => haveInjector
                ? ApplicationNotificationManager.Manager.AskForChoice("Plugin not loaded", message, "Load now", "Restart the game", "Later")
                : ApplicationNotificationManager.Manager.AskForChoice("Plugin not loaded", message, "Restart the game", "Later"));

            var action = haveInjector ? choice : choice + 1;

            switch (action)
            {
                case 0:
                    if (!await RunInjectorAsync(injectorPath, pid, pluginPath))
                    {
                        ApplicationNotificationManager.Manager.ShowError("Plugin",
                            "InjectHelper.exe could not load the plugin into Audiosurf - see the log for details.");
                        return;
                    }

                    SetLinkState(PluginLinkState.Waiting);
                    if (!await WaitForPluginReadyAsync(pid, ReadyTimeout))
                    {
                        SetLinkState(PluginLinkState.NotLoadedInSession);
                        ApplicationNotificationManager.Manager.ShowError("Plugin",
                            "The plugin was loaded into Audiosurf but never became ready.");
                        return;
                    }

                    await BeginHandshakeAsync(HandshakeTimeout, HandshakeAttempts);
                    return;

                case 1:
                    // The restarted game raises Registered again and comes back through ConnectAsync with the
                    // plugin loaded from channels\ - there is nothing to follow up on from here.
                    if (await GameProcessService.CloseAsync())
                        GameProcessService.Start(PluginService.GameRootDirectory);
                    return;

                default:
                    return;
            }
        }

        /// <summary>
        /// A plugin that is demonstrably loaded yet will not answer a handshake is a plugin whose hooks
        /// are already live inside the game with no way to reach them. It is not just the overlay that
        /// is suspect at that point - the plugin sits on the game's render loop, WndProc and input - so
        /// the interface stops entirely rather than keep driving a process in that state.
        /// </summary>
        private static async Task SuspendForStalePluginAsync(string detail)
        {
            SetLinkState(PluginLinkState.Idle);

            var reason =
                $"A Tweaker overlay plugin is already loaded into Audiosurf ({detail}), but it does not respond. " +
                "The Audiosurf service has been stopped, because a plugin in that state leaves the rest of the game " +
                "process suspect too. Restart Audiosurf, then press Reset next to the connection status.";

            Logger.Log("OverlayHelper", reason);

            // SuspendService kills the bridge subprocess and joins its pump thread - up to ~2s of
            // blocking, and this continuation runs on the UI thread (same reason MainWindowViewModel's
            // ResetBridge command offloads its own call).
            await Task.Run(() => AudiosurfHandle.Instance.SuspendService(reason));
        }

        private static async Task<bool> RunInjectorAsync(string injectorPath, int pid, string pluginPath)
        {
            if (string.IsNullOrEmpty(pluginPath) || !File.Exists(pluginPath))
            {
                Logger.Log("OverlayHelper", $"Plugin DLL '{pluginPath}' is not where it should be - nothing to load.");
                return false;
            }

            var startInfo = new ProcessStartInfo(injectorPath, $"{pid} \"{pluginPath}\"")
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            try
            {
                using var process = Process.Start(startInfo);
                var stderrTask = process.StandardError.ReadToEndAsync();
                var stdoutTask = process.StandardOutput.ReadToEndAsync();
                await process.WaitForExitAsync();

                if (process.ExitCode != 0)
                {
                    var stderr = await stderrTask;
                    var stdout = await stdoutTask;
                    Logger.Log("OverlayHelper",
                        $"InjectHelper.exe exited with code {process.ExitCode}. stderr: {stderr} stdout: {stdout}");
                    return false;
                }

                return true;
            }
            catch (Exception ex)
            {
                Logger.Log("OverlayHelper", $"Failed to start InjectHelper.exe: {ex}");
                return false;
            }
        }

        private enum HandshakeOutcome
        {
            Connected,
            NoAnswer,
            ProtocolMismatch
        }

        /// <summary>
        /// Sends HANDSHAKE_BEGIN and waits for the ack, up to <paramref name="attempts"/> times (§6.5). Retrying
        /// is worth it because the plugin can be ready in the sense that matters here - it has a window - a
        /// moment before its IPC subscribers are published, and a single missed round trip would otherwise cost
        /// the whole session.
        /// </summary>
        private static async Task<HandshakeOutcome> BeginHandshakeAsync(TimeSpan timeout, int attempts)
        {
            for (var attempt = 1; attempt <= attempts; attempt++)
            {
                var tcs = new TaskCompletionSource<HandshakeAck>(TaskCreationOptions.RunContinuationsAsynchronously);
                lock (_lock)
                    _handshakeAckTcs = tcs;

                AudiosurfHandle.Instance.OverlayCommand($"HANDSHAKE_BEGIN {AudiosurfHandle.Instance.ListenerWindowCaption}");

                var completed = await Task.WhenAny(tcs.Task, Task.Delay(timeout));
                if (completed != tcs.Task)
                {
                    Logger.Log("OverlayHelper",
                        $"No HANDSHAKE_ACK from TweakerPlugin within {timeout.TotalMilliseconds:F0} ms (attempt {attempt}/{attempts}).");
                    lock (_lock)
                        _handshakeAckTcs = null;
                    continue;
                }

                var ack = await tcs.Task;

                if (ack.Protocol != ExpectedProtocolVersion)
                {
                    Logger.Log("OverlayHelper",
                        $"TweakerPlugin v{ack.Version} speaks protocol {ack.Protocol}, this host speaks {ExpectedProtocolVersion}.");
                    SetLinkState(PluginLinkState.ProtocolMismatch, ack.Version, ack.LoadMode);
                    return HandshakeOutcome.ProtocolMismatch;
                }

                Logger.Log("OverlayHelper", $"Connected to TweakerPlugin v{ack.Version} (protocol {ack.Protocol}, loaded from {ack.LoadMode}).");

                lock (_lock)
                    _ready = true;

                SetLinkState(PluginLinkState.Connected, ack.Version, ack.LoadMode);

                PushTweakSnapshot();
                OverlayReady?.Invoke(null, EventArgs.Empty);
                return HandshakeOutcome.Connected;
            }

            return HandshakeOutcome.NoAnswer;
        }

        private static void SetLinkState(PluginLinkState state, string version = null, string loadMode = null)
        {
            lock (_lock)
            {
                if (_linkState == state && _connectedVersion == version && _connectedLoadMode == loadMode)
                    return;

                _linkState = state;
                _connectedVersion = version;
                _connectedLoadMode = loadMode;
            }

            LinkStateChanged?.Invoke(null, EventArgs.Empty);
        }

        private static void OnGameConfigStateChanged(object sender, GameConfigStateChangedEventArgs e)
        {
            PushTweak(e.Snapshot);
        }

        private static void OnOverlayMessageReceived(object sender, string content)
        {
            if (content.StartsWith(HandshakeAckPayload, StringComparison.Ordinal))
            {
                TaskCompletionSource<HandshakeAck> tcs;
                lock (_lock)
                {
                    tcs = _handshakeAckTcs;
                    _handshakeAckTcs = null;
                }

                tcs?.TrySetResult(ParseAck(content));
                return;
            }

            if (content.StartsWith(NotifyTweakPrefix, StringComparison.Ordinal))
            {
                // "<WireName> <true|false>" - same fixed shape as TWEAK_SET's own tokens, just the
                // reverse direction (see overlay-protocol.md's L3 token table).
                var rest = content.Substring(NotifyTweakPrefix.Length);
                var spaceIndex = rest.IndexOf(' ');
                if (spaceIndex < 0)
                    return;

                var wireName = rest.Substring(0, spaceIndex);
                var valueToken = rest.Substring(spaceIndex + 1);
                var enabled = valueToken == "true" || valueToken == "1";
                TweakRequested?.Invoke(null, new TweakRequestedEventArgs(wireName, enabled));
                return;
            }

            if (content.StartsWith(NotifySkinPrefix, StringComparison.Ordinal))
            {
                var name = Uri.UnescapeDataString(content.Substring(NotifySkinPrefix.Length));
                SkinRequested?.Invoke(null, name);
                return;
            }

            if (content.StartsWith(QuickPlayerNotifyPrefix, StringComparison.Ordinal))
                QuickPlayerRequested?.Invoke(null, content);
        }

        /// <summary>
        /// "HANDSHAKE_ACK &lt;version&gt; &lt;protocol&gt; &lt;load_mode&gt;" (§5.2). A bare ack with no tokens is
        /// read as protocol 0 - an older plugin than any this host can talk to, which is exactly the mismatch
        /// path, not a parse error to swallow.
        /// </summary>
        private static HandshakeAck ParseAck(string content)
        {
            var tokens = content.Split(' ', StringSplitOptions.RemoveEmptyEntries);

            var version = tokens.Length > 1 ? tokens[1] : "unknown";
            var protocol = tokens.Length > 2 && int.TryParse(tokens[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out var parsed)
                ? parsed
                : 0;
            var loadMode = tokens.Length > 3 ? tokens[3] : "unknown";

            return new HandshakeAck(version, protocol, loadMode);
        }

        private readonly record struct HandshakeAck(string Version, int Protocol, string LoadMode);

        private const uint SYNCHRONIZE = 0x00100000;
        private const int ERROR_ACCESS_DENIED = 5;

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, EntryPoint = "OpenMutexW", SetLastError = true)]
        private static extern IntPtr OpenMutex(uint desiredAccess, bool inheritHandle, string name);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, EntryPoint = "OpenEventW", SetLastError = true)]
        private static extern IntPtr OpenEvent(uint desiredAccess, bool inheritHandle, string name);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr handle);
    }

    internal readonly record struct TweakRequestedEventArgs(string WireName, bool Enabled);

    /// <summary>
    /// How far the host has got with the plugin inside the running game. Only the states the status line (§6.6)
    /// has to tell apart - "not installed" and "installed but the game is not running" are questions about the
    /// disk and the process list, answered by PluginService rather than tracked here.
    /// </summary>
    internal enum PluginLinkState
    {
        /// <summary>No game, no plugin in it, or syncing is off.</summary>
        Idle,

        /// <summary>The plugin is in the game process; waiting for it to be ready, or handshaking.</summary>
        Waiting,

        Connected,

        /// <summary>It answered, in a protocol this host does not speak.</summary>
        ProtocolMismatch,

        /// <summary>Installed on disk, but this session of the game started without it.</summary>
        NotLoadedInSession
    }
}
