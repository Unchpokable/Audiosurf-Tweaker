using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using AudiosurfInterface;

namespace TweakerUI.Core
{
    /// <summary>
    /// Drives the TW_OVL host pipeline (Docs/Internal/overlay-protocol.md): once AudiosurfHandle
    /// reports the game connected, injects TweakerPlugin.dll (via InjectHelper.exe), performs the
    /// L3 handshake over the already-generic OVERLAY_SEND channel, then pushes an initial snapshot
    /// and stays open for live updates. TweakerCore intentionally doesn't know about
    /// AudiosurfInterface (see overview.md), so this lives here rather than there.
    /// </summary>
    internal static class OverlayHelper
    {
        private const string InjectorFileName = "InjectHelper.exe";
        private const string PluginFileName = "TweakerPlugin.dll";
        private const string HandshakeAckPayload = "HANDSHAKE_ACK";
        private const string NotifyTweakPrefix = "NOTIFY_TWEAK ";
        private const string NotifySkinPrefix = "NOTIFY_SKIN ";

        // Quick Player owns a whole family of ops (Docs/Internal/overlay-quickplayer.md). Only the
        // prefix is known here - the grammar itself lives in QuickPlayerOverlayBridge, the same way
        // the plugin hands QP_* straight to its own module instead of parsing it in ipc/overlay_ipc.
        private const string QuickPlayerNotifyPrefix = "QP_NOTIFY_";

        // Must match TweakerPlugin's claim_single_instance() (src/plugin/load.cxx) exactly, including
        // the deliberate absence of any version or path component - the point is that a newer host can
        // still recognise an older, possibly renamed plugin sitting in the game.
        private const string OverlayInstanceMutexPrefix = @"Local\AudiosurfTweaker.Overlay.";

        private static readonly TimeSpan HandshakeTimeout = TimeSpan.FromSeconds(5);

        // Shorter than the real handshake: this one runs against a process where nothing was detected,
        // so the expected outcome is silence, and every millisecond of it delays a cold-start
        // injection. Long enough to cover a WM_COPYDATA round trip through asbridge on a loaded machine.
        private static readonly TimeSpan ProbeTimeout = TimeSpan.FromMilliseconds(1500);

        private static readonly Logger _logger = new Logger();
        private static readonly object _lock = new object();

        private static bool _initialized;
        private static bool _ready;
        private static TaskCompletionSource<bool> _handshakeAckTcs;

        /// <summary>
        /// Fired once the L3 handshake is acked and the channel is ready for live pushes.
        /// OverlayHelper pushes the GameConfigState tweak snapshot itself; other domains such as
        /// SkinChangerViewModel subscribe to push their own initial state.
        /// </summary>
        internal static event EventHandler OverlayReady;

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

        internal static void Initialize()
        {
            if (_initialized)
                return;
            _initialized = true;

            AudiosurfHandle.Instance.Registered += OnRegistered;
            AudiosurfHandle.Instance.OverlayMessageReceived += OnOverlayMessageReceived;
            GameConfigState.Manager.StateChanged += OnGameConfigStateChanged;
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
            if (!SettingsProvider.EnableInGameOverlay)
                return;

            _ = EnsureOverlayInjectedAsync();
        }

        private static async Task EnsureOverlayInjectedAsync()
        {
            lock (_lock)
                _ready = false;

            var pid = AudiosurfHandle.Instance.GamePID;
            if (pid == 0)
                return;

            // Environment.ProcessPath (not AppDomain.CurrentDomain.BaseDirectory) - the latter resolves
            // into the self-extraction temp folder under single-file publish, not the folder that
            // actually holds InjectHelper.exe/TweakerPlugin.dll. Same fix as LegacyConverter.GetConverterPath.
            var baseDir = Path.GetDirectoryName(Environment.ProcessPath);
            var pluginPath = Path.Combine(baseDir, PluginFileName);
            var injectorPath = Path.Combine(baseDir, InjectorFileName);

            if (!File.Exists(pluginPath))
            {
                _logger.Log("OverlayHelper", $"TweakerPlugin.dll not found at '{pluginPath}' - overlay not injected.");
                return;
            }

            if (!File.Exists(injectorPath))
            {
                _logger.Log("OverlayHelper", $"InjectHelper.exe not found at '{injectorPath}' - overlay not injected.");
                return;
            }

            // Inject guard (Docs/Internal/overlay-protocol.md, "Защита от повторного инжекта"). A
            // second copy of the plugin in one process means a second set of Detours over EndScene/
            // CallChannel - a crash, not a degraded overlay - so injection is the last resort, only
            // reached once nothing answers and nothing is detectably loaded.
            var presence = await Task.Run(() => DetectLoadedPlugin(pid, pluginPath));

            if (presence.IsPresent)
            {
                // Adopt rather than inject. HANDSHAKE_BEGIN doubles as the adoption mechanism: it makes
                // the plugin re-resolve the bridge window by caption, which re-points a plugin left
                // over from a previous Tweaker session (or from a bridge we have since restarted) at
                // our current asbridge instance.
                if (await BeginHandshakeAsync(HandshakeTimeout))
                    return;

                await SuspendForStalePluginAsync(presence.Detail);
                return;
            }

            // Detection can prove presence but never absence: a plugin built before the instance mutex
            // existed and renamed on disk leaves nothing this side can see. One short probe against an
            // otherwise-clean process is the difference between adopting such a plugin and stacking a
            // second set of hooks on top of it.
            if (await BeginHandshakeAsync(ProbeTimeout))
            {
                _logger.Log("OverlayHelper",
                    "An undetected but responsive overlay plugin answered the probe - adopted it instead of injecting.");
                return;
            }

            if (!await RunInjectorAsync(injectorPath, pid, pluginPath))
                return;

            await Task.Delay(1500); // Give internal plugin some time to initialize

            await BeginHandshakeAsync(HandshakeTimeout);
        }

        /// <summary>
        /// Three independent presence signals, cheapest first. The instance mutex is the only
        /// rename-proof one and is what a current plugin build always publishes; the two module checks
        /// stay as the fallback for plugins built before it existed.
        /// </summary>
        private static PluginPresence DetectLoadedPlugin(int pid, string expectedPluginPath)
        {
            if (IsOverlayInstanceMutexPresent(pid, out var mutexDetail))
                return new PluginPresence(true, mutexDetail);

            return DetectPluginModule(pid, expectedPluginPath);
        }

        private static bool IsOverlayInstanceMutexPresent(int pid, out string detail)
        {
            detail = null;

            var handle = OpenMutex(SYNCHRONIZE, false, OverlayInstanceMutexPrefix + pid.ToString());
            if (handle != IntPtr.Zero)
            {
                CloseHandle(handle);
                detail = "its single-instance mutex is held in the game process";
                return true;
            }

            // Distinguishing the two failures matters: "no such object" is a genuine absence, while
            // "access denied" means the object exists and belongs to a context this process cannot
            // touch. Treating the latter as absence would inject a second copy on top of a live one.
            var error = Marshal.GetLastWin32Error();
            if (error == ERROR_ACCESS_DENIED)
            {
                detail = "its single-instance mutex exists but is not accessible from this process";
                return true;
            }

            return false;
        }

        private static PluginPresence DetectPluginModule(int pid, string expectedPluginPath)
        {
            var expectedFileName = Path.GetFileName(expectedPluginPath);

            try
            {
                using var process = Process.GetProcessById(pid);

                var modules = new IntPtr[1024];
                if (!EnumProcessModules(process.Handle, modules, (uint)(modules.Length * IntPtr.Size), out var bytesNeeded))
                    return PluginPresence.None;

                var moduleCount = (int)(bytesNeeded / IntPtr.Size);
                var builder = new StringBuilder(1024);

                for (var i = 0; i < moduleCount; i++)
                {
                    builder.Clear();
                    if (GetModuleFileNameEx(process.Handle, modules[i], builder, (uint)builder.Capacity) == 0)
                        continue;

                    var modulePath = builder.ToString();
                    if (string.Equals(modulePath, expectedPluginPath, StringComparison.OrdinalIgnoreCase))
                        return new PluginPresence(true, "it is loaded from this Tweaker's own folder");

                    // A copy from another install (or an older Tweaker's Plugins folder) is still a
                    // plugin holding the same hooks - the path differing does not make it safe to add
                    // a second one.
                    if (string.Equals(Path.GetFileName(modulePath), expectedFileName, StringComparison.OrdinalIgnoreCase))
                        return new PluginPresence(true, $"a copy is loaded from '{modulePath}'");
                }
            }
            catch (Exception ex) when (ex is ArgumentException or InvalidOperationException)
            {
                // Process already exited between GamePID being read and this check running.
            }

            return PluginPresence.None;
        }

        /// <summary>
        /// A plugin that is demonstrably loaded yet will not answer a handshake is a plugin whose hooks
        /// are already live inside the game with no way to reach them. It is not just the overlay that
        /// is suspect at that point - the plugin sits on the game's render loop, WndProc and input - so
        /// the interface stops entirely rather than keep driving a process in that state.
        /// </summary>
        private static async Task SuspendForStalePluginAsync(string detail)
        {
            var reason =
                $"A Tweaker overlay plugin is already loaded into Audiosurf ({detail}), but it does not respond. " +
                "The Audiosurf service has been stopped, because a plugin in that state leaves the rest of the game " +
                "process suspect too. Restart Audiosurf, then press Reset next to the connection status.";

            _logger.Log("OverlayHelper", reason);

            // SuspendService kills the bridge subprocess and joins its pump thread - up to ~2s of
            // blocking, and this continuation runs on the UI thread (same reason MainWindowViewModel's
            // ResetBridge command offloads its own call).
            await Task.Run(() => AudiosurfHandle.Instance.SuspendService(reason));
        }

        private static async Task<bool> RunInjectorAsync(string injectorPath, int pid, string pluginPath)
        {
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
                await Task.Run(() => process.WaitForExit());

                if (process.ExitCode != 0)
                {
                    var stderr = await stderrTask;
                    var stdout = await stdoutTask;
                    _logger.Log("OverlayHelper",
                        $"InjectHelper.exe exited with code {process.ExitCode}. stderr: {stderr} stdout: {stdout}");
                    return false;
                }

                return true;
            }
            catch (Exception ex)
            {
                _logger.Log("OverlayHelper", $"Failed to start InjectHelper.exe: {ex}");
                return false;
            }
        }

        /// <summary>
        /// Sends HANDSHAKE_BEGIN and waits for the ack. Doubles as the liveness probe of the whole
        /// inject guard, hence the caller-supplied timeout: a probe against a process believed to be
        /// clean should not cost the full handshake budget. Returns whether the channel came up.
        /// </summary>
        private static async Task<bool> BeginHandshakeAsync(TimeSpan timeout)
        {
            var tcs = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
            lock (_lock)
                _handshakeAckTcs = tcs;

            AudiosurfHandle.Instance.OverlayCommand($"HANDSHAKE_BEGIN {AudiosurfHandle.Instance.ListenerWindowCaption}");

            var completed = await Task.WhenAny(tcs.Task, Task.Delay(timeout));
            if (completed != tcs.Task)
            {
                _logger.Log("OverlayHelper", $"No HANDSHAKE_ACK from TweakerPlugin within {timeout.TotalMilliseconds:F0} ms.");
                lock (_lock)
                    _handshakeAckTcs = null;
                return false;
            }

            lock (_lock)
                _ready = true;

            PushTweakSnapshot();
            OverlayReady?.Invoke(null, EventArgs.Empty);
            return true;
        }

        private static void OnGameConfigStateChanged(object sender, GameConfigStateChangedEventArgs e)
        {
            PushTweak(e.Snapshot);
        }

        private static void OnOverlayMessageReceived(object sender, string content)
        {
            if (string.Equals(content, HandshakeAckPayload, StringComparison.Ordinal))
            {
                TaskCompletionSource<bool> tcs;
                lock (_lock)
                {
                    tcs = _handshakeAckTcs;
                    _handshakeAckTcs = null;
                }

                tcs?.TrySetResult(true);
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

        private const uint SYNCHRONIZE = 0x00100000;
        private const int ERROR_ACCESS_DENIED = 5;

        [DllImport("psapi.dll")]
        private static extern bool EnumProcessModules(IntPtr hProcess, IntPtr[] lphModule, uint cb, out uint lpcbNeeded);

        [DllImport("psapi.dll", CharSet = CharSet.Unicode)]
        private static extern uint GetModuleFileNameEx(IntPtr hProcess, IntPtr hModule, StringBuilder lpBaseName, uint nSize);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, EntryPoint = "OpenMutexW", SetLastError = true)]
        private static extern IntPtr OpenMutex(uint desiredAccess, bool inheritHandle, string name);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr handle);
    }

    internal readonly record struct TweakRequestedEventArgs(string WireName, bool Enabled);

    /// <summary>
    /// Outcome of the inject guard's detection pass. <see cref="Detail"/> is the human-readable half -
    /// it ends up verbatim in the message the user sees when a stale plugin forces a suspension, so
    /// they can tell "your own copy went stale" apart from "something else's copy is in there".
    /// </summary>
    internal readonly record struct PluginPresence(bool IsPresent, string Detail)
    {
        internal static PluginPresence None => new PluginPresence(false, null);
    }
}
