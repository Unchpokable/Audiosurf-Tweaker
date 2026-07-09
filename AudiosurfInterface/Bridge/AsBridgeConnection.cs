using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;

namespace AudiosurfInterface.Bridge
{
    /// <summary>
    /// Owns the asbridge.exe subprocess and the named pipe to it: spawns the bridge (which itself
    /// finds the game window and does all the Win32 messaging), keeps a background read loop over
    /// the pipe, and restarts the whole chain if the subprocess dies or the pipe breaks. The
    /// tweaker's UI process never touches Win32 window APIs itself.
    /// </summary>
    public sealed class AsBridgeConnection : IDisposable
    {
        private const string BridgeExecutableName = "asbridge.exe";
        private const int ReadBufferSize = 4096;
        private const int PipeConnectTimeoutMs = 5000;
        private const int RestartDelayMs = 1000;

        public event Action<AsBridgeReport> ReportReceived;
        public event Action ConnectionEstablished;
        public event Action ConnectionLost;

        public bool IsConnected => _pipe != null && _pipe.IsConnected;
        public string ListenerWindowCaption { get; }

        private readonly string _pipeName;
        private readonly object _writeLock = new object();

        private Process _bridgeProcess;
        private NamedPipeClientStream _pipe;
        private Thread _pumpThread;
        private CancellationTokenSource _lifetime;
        private bool _disposed;

        public AsBridgeConnection(string listenerWindowCaption)
        {
            ListenerWindowCaption = listenerWindowCaption;
            _pipeName = $"asbridge-{Guid.NewGuid():N}";
        }

        public static bool IsBridgeAvailable => File.Exists(GetBridgePath());

        public void Start()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(AsBridgeConnection));
            if (_lifetime != null)
                return;

            _lifetime = new CancellationTokenSource();
            _pumpThread = new Thread(() => PumpLoop(_lifetime.Token))
            {
                IsBackground = true,
                Name = "asbridge-pump"
            };
            _pumpThread.Start();
        }

        public bool Send(string command)
        {
            var pipe = _pipe;
            if (pipe == null || !pipe.IsConnected)
                return false;

            try
            {
                var payload = Encoding.UTF8.GetBytes(AsBridgeProtocol.SerializeSend(command));
                lock (_writeLock)
                {
                    pipe.Write(payload, 0, payload.Length);
                    pipe.Flush();
                }
                return true;
            }
            catch (Exception ex) when (ex is IOException || ex is ObjectDisposedException || ex is InvalidOperationException)
            {
                return false;
            }
        }

        private void PumpLoop(CancellationToken token)
        {
            while (!token.IsCancellationRequested)
            {
                try
                {
                    EnsureBridgeProcess();
                    ConnectPipe(token);

                    ConnectionEstablished?.Invoke();
                    ReadUntilBroken(token);
                }
                catch (OperationCanceledException)
                {
                    return;
                }
                catch
                {
                    // Any failure - bridge missing, spawn failure, pipe refusing to connect -
                    // degrades to "not connected, retry shortly" rather than surfacing anywhere;
                    // the game simply shows as disconnected until the chain comes back.
                }

                TeardownPipe();
                if (token.IsCancellationRequested)
                    return;

                ConnectionLost?.Invoke();
                token.WaitHandle.WaitOne(RestartDelayMs);
            }
        }

        private void EnsureBridgeProcess()
        {
            if (_bridgeProcess != null && !_bridgeProcess.HasExited)
                return;

            _bridgeProcess?.Dispose();

            var bridgePath = GetBridgePath();
            if (!File.Exists(bridgePath))
                throw new FileNotFoundException("asbridge.exe not found next to the tweaker executable", bridgePath);

            var ownPid = Process.GetCurrentProcess().Id;
            _bridgeProcess = Process.Start(new ProcessStartInfo
            {
                FileName = bridgePath,
                // Window title, full pipe path (asbridge passes it verbatim to CreateNamedPipeW),
                // and our PID for the bridge's orphan protection.
                Arguments = $"{ListenerWindowCaption} \\\\.\\pipe\\{_pipeName} {ownPid}",
                UseShellExecute = false,
                CreateNoWindow = true
            });
        }

        private void ConnectPipe(CancellationToken token)
        {
            var pipe = new NamedPipeClientStream(".", _pipeName, PipeDirection.InOut, PipeOptions.Asynchronous);
            try
            {
                pipe.Connect(PipeConnectTimeoutMs);
                pipe.ReadMode = PipeTransmissionMode.Message;
            }
            catch
            {
                pipe.Dispose();
                throw;
            }

            token.ThrowIfCancellationRequested();
            _pipe = pipe;
        }

        private void ReadUntilBroken(CancellationToken token)
        {
            var buffer = new byte[ReadBufferSize];
            var message = new MemoryStream();

            while (!token.IsCancellationRequested)
            {
                int read = _pipe.Read(buffer, 0, buffer.Length);
                if (read <= 0)
                    return; // peer gone

                message.Write(buffer, 0, read);
                if (!_pipe.IsMessageComplete)
                    continue;

                var raw = Encoding.UTF8.GetString(message.GetBuffer(), 0, (int)message.Length);
                message.SetLength(0);

                if (AsBridgeProtocol.TryParseReport(raw, out var report))
                    ReportReceived?.Invoke(report);
            }
        }

        private void TeardownPipe()
        {
            var pipe = _pipe;
            _pipe = null;
            pipe?.Dispose();
        }

        private static string GetBridgePath()
        {
            return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, BridgeExecutableName);
        }

        public void Dispose()
        {
            if (_disposed)
                return;
            _disposed = true;

            _lifetime?.Cancel();
            TeardownPipe(); // unblocks a pending Read so the pump thread can exit
            _pumpThread?.Join(TimeSpan.FromSeconds(2));
            _lifetime?.Dispose();

            try
            {
                if (_bridgeProcess != null && !_bridgeProcess.HasExited)
                    _bridgeProcess.Kill();
            }
            catch (Exception ex) when (ex is InvalidOperationException || ex is System.ComponentModel.Win32Exception)
            {
                // Already exited or inaccessible - nothing to clean up.
            }

            _bridgeProcess?.Dispose();
        }
    }
}
