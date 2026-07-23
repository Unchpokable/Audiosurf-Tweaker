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

        private static readonly TimeSpan HandshakeTimeout = TimeSpan.FromSeconds(5);
        private static readonly Logger _logger = new Logger();
        private static readonly object _lock = new object();

        private static bool _initialized;
        private static bool _ready;
        private static TaskCompletionSource<bool> _handshakeAckTcs;

        /// <summary>
        /// Fired once the L3 handshake is acked and the channel is ready for live pushes -
        /// SkinChangerViewModel/TweakerViewModel subscribe to push their current state as the
        /// initial snapshot, the same methods they call afterwards on every live change.
        /// </summary>
        internal static event EventHandler OverlayReady;

        /// <summary>
        /// Reverse-sync (Docs/Internal/overlay-protocol.md, "Reverse-sync: NOTIFY_TWEAK/NOTIFY_SKIN"):
        /// the user clicked a tweak/skin directly in the in-game overlay. TweakerViewModel/
        /// SkinChangerViewModel subscribe and apply the request through the exact same setters/methods
        /// the desktop UI itself uses - that's what makes those setters' existing SetTweak/
        /// PushCurrentSkin re-push act as the plugin's confirmation, with no separate ACK op needed.
        /// OverlayHelper deliberately doesn't call into the ViewModels itself (would invert the
        /// dependency direction every other OverlayHelper touchpoint uses - see OverlayReady above).
        /// </summary>
        internal static event EventHandler<TweakRequestedEventArgs> TweakRequested;
        internal static event EventHandler<string> SkinRequested;

        internal static void Initialize()
        {
            if (_initialized)
                return;
            _initialized = true;

            AudiosurfHandle.Instance.Registered += OnRegistered;
            AudiosurfHandle.Instance.OverlayMessageReceived += OnOverlayMessageReceived;
        }

        internal static void SetTweak(string wireName, bool enabled)
        {
            if (!IsReady)
                return;

            AudiosurfHandle.Instance.OverlayCommand($"TWEAK_SET {wireName} {(enabled ? "true" : "false")}");
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

            var alreadyLoaded = await Task.Run(() => IsPluginLoaded(pid, pluginPath));
            if (!alreadyLoaded && !await RunInjectorAsync(injectorPath, pid, pluginPath))
                return;

            await BeginHandshakeAsync();
        }

        private static bool IsPluginLoaded(int pid, string pluginPath)
        {
            try
            {
                using var process = Process.GetProcessById(pid);

                var modules = new IntPtr[1024];
                if (!EnumProcessModules(process.Handle, modules, (uint)(modules.Length * IntPtr.Size), out var bytesNeeded))
                    return false;

                var moduleCount = (int)(bytesNeeded / IntPtr.Size);
                var builder = new StringBuilder(1024);

                for (var i = 0; i < moduleCount; i++)
                {
                    builder.Clear();
                    if (GetModuleFileNameEx(process.Handle, modules[i], builder, (uint)builder.Capacity) > 0
                        && string.Equals(builder.ToString(), pluginPath, StringComparison.OrdinalIgnoreCase))
                    {
                        return true;
                    }
                }
            }
            catch (Exception ex) when (ex is ArgumentException or InvalidOperationException)
            {
                // Process already exited between GamePID being read and this check running.
            }

            return false;
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

        private static async Task BeginHandshakeAsync()
        {
            var tcs = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
            lock (_lock)
                _handshakeAckTcs = tcs;

            AudiosurfHandle.Instance.OverlayCommand($"HANDSHAKE_BEGIN {AudiosurfHandle.Instance.ListenerWindowCaption}");

            var completed = await Task.WhenAny(tcs.Task, Task.Delay(HandshakeTimeout));
            if (completed != tcs.Task)
            {
                _logger.Log("OverlayHelper", "Timed out waiting for HANDSHAKE_ACK from TweakerPlugin.");
                lock (_lock)
                    _handshakeAckTcs = null;
                return;
            }

            lock (_lock)
                _ready = true;

            OverlayReady?.Invoke(null, EventArgs.Empty);
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
            }
        }

        [DllImport("psapi.dll")]
        private static extern bool EnumProcessModules(IntPtr hProcess, IntPtr[] lphModule, uint cb, out uint lpcbNeeded);

        [DllImport("psapi.dll", CharSet = CharSet.Unicode)]
        private static extern uint GetModuleFileNameEx(IntPtr hProcess, IntPtr hModule, StringBuilder lpBaseName, uint nSize);
    }

    internal readonly record struct TweakRequestedEventArgs(string WireName, bool Enabled);
}
