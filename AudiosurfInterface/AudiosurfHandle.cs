using System;
using System.Collections.Generic;
using System.Threading;
using AudiosurfInterface.Bridge;

namespace AudiosurfInterface
{
    public delegate void MessageEventHandler(object sender, string messageContent);

    /// <summary>
    /// Facade over the asbridge.exe subprocess. Keeps the public surface the WPF side has always
    /// consumed (state events, command queue, listener caption), but all actual Win32 work - game
    /// window discovery, WM_COPYDATA exchange, listener registration - lives in the native bridge;
    /// this class only talks the pipe protocol (see AsBridgeProtocol).
    /// </summary>
    public class AudiosurfHandle : IDisposable
    {
        private AudiosurfHandle()
        {
            _currentState = ASHandleState.NotConnected;
            _queuedCommands = new Queue<string>();

            // The legacy WndProc listener delivered everything on the UI thread; the pipe pump is a
            // background thread. Capture the creating thread's context (the singleton is first
            // touched from the UI) so subscribers keep seeing events on the thread they always did.
            _syncContext = SynchronizationContext.Current;

            var caption = "AsMsgHandler_" + Convert.ToBase64String(Guid.NewGuid().ToByteArray()).Substring(0, 5);
            _connection = new AsBridgeConnection(caption);
            _connection.ReportReceived += OnReportReceived;
            _connection.ConnectionLost += OnBridgeConnectionLost;
            _connection.Start();
        }

        public event EventHandler StateChanged;
        public event EventHandler Registered;
        public event MessageEventHandler MessageResieved;
        public event EventHandler<CommandInfo> CommandSent;
        public event EventHandler MessageServiceInitialized;

        public bool IsValid { get; private set; }
        public int GamePID { get; private set; }

        public string ListenerWindowCaption => _connection.ListenerWindowCaption;

        public string StateMessage => _currentState.Message;
        public string StateColor => _currentState.ColorInterpretation;

        private AsBridgeConnection _connection;
        private ASHandleState _currentState;
        private readonly Queue<string> _queuedCommands;
        private readonly SynchronizationContext _syncContext;
        private static AudiosurfHandle _instance;

        private readonly object _lockObject = new object();

        public static AudiosurfHandle Instance
        {
            get
            {
                if (_instance != null) return _instance;
                _instance = new AudiosurfHandle();
                return _instance;
            }
        }

        /// <summary>
        /// Tears down and recreates the bridge subprocess + pipe. Historically this recreated the
        /// WndProc listener window; the semantic - "reset the whole IPC channel" - is unchanged.
        /// </summary>
        public bool ReinitializeWndProcMessageService()
        {
            lock (_lockObject)
            {
                try
                {
                    IsValid = false;
                    GamePID = 0;
                    _currentState = ASHandleState.NotConnected;
                    StateChanged?.Invoke(this, EventArgs.Empty);

                    _connection.ReportReceived -= OnReportReceived;
                    _connection.ConnectionLost -= OnBridgeConnectionLost;
                    _connection.Dispose();

                    var caption = "AsMsgHandler_" + Convert.ToBase64String(Guid.NewGuid().ToByteArray()).Substring(0, 5);
                    _connection = new AsBridgeConnection(caption);
                    _connection.ReportReceived += OnReportReceived;
                    _connection.ConnectionLost += OnBridgeConnectionLost;
                    _connection.Start();

                    MessageServiceInitialized?.Invoke(this, EventArgs.Empty);
                    return true;
                }
                catch { return false; }
            }
        }

        // The bridge subprocess owns game discovery and reconnects on its own 30ms timer; these
        // remain as no-ops so the calling code (auto-handling toggles, manual reconnect button)
        // keeps compiling and behaving sensibly.
        public void StopAutoHandling()
        {
        }

        public void StartAutoHandling()
        {
        }

        public bool TryConnect()
        {
            return IsValid;
        }

        public void Command(string message)
        {
            lock (_lockObject)
            {
                if (_currentState != ASHandleState.Connected)
                {
                    if (message.Contains("reloadtextures")) return; //No need to enqueue reloadtextures command
                    _queuedCommands.Enqueue(message);
                    CommandSent?.Invoke(this, new CommandInfo(message, CommandInfo.CommandStatus.Enqueued));
                    return;
                }

                var delivered = _connection.Send(message);
                CommandSent?.Invoke(this,
                    new CommandInfo(message, delivered ? CommandInfo.CommandStatus.Sent : CommandInfo.CommandStatus.Enqueued));
                if (!delivered && !message.Contains("reloadtextures"))
                    _queuedCommands.Enqueue(message);
            }
        }

        private void OnReportReceived(AsBridgeReport report)
        {
            Dispatch(() => HandleReport(report));
        }

        private void Dispatch(Action action)
        {
            if (_syncContext != null)
                _syncContext.Post(_ => action(), null);
            else
                action();
        }

        private void HandleReport(AsBridgeReport report)
        {
            lock (_lockObject)
            {
                switch (report.Type)
                {
                    case AsBridgeReportType.Service:
                        HandleServiceReport(report);
                        break;

                    case AsBridgeReportType.BroadcastForward:
                        HandleGameBroadcast(report.Details.Count > 0 ? report.Details[0] : string.Empty);
                        break;

                    case AsBridgeReportType.Ok:
                    case AsBridgeReportType.Failed:
                        // Per-command acks; the console already logs the Sent event, nothing to do.
                        break;
                }
            }
        }

        private void HandleServiceReport(AsBridgeReport report)
        {
            if (report.Details.Count == 0)
                return;

            switch (report.Details[0])
            {
                case AsBridgeProtocol.ServiceStatusWindowFound:
                    // The bridge has already sent the registration command to the game itself;
                    // "connected" is confirmed later by the game's broadcast reply. The game answers
                    // registration synchronously, so the broadcast may even arrive first - never
                    // downgrade an already-Connected state back to Awaiting here.
                    if (report.Details.Count > 1 && int.TryParse(report.Details[1], out var pid))
                        GamePID = pid;
                    if (_currentState != ASHandleState.Connected)
                        _currentState = ASHandleState.Awaiting;
                    IsValid = true;
                    StateChanged?.Invoke(this, EventArgs.Empty);
                    break;

                case AsBridgeProtocol.ServiceStatusWindowLost:
                    GamePID = 0;
                    IsValid = false;
                    _currentState = ASHandleState.NotConnected;
                    StateChanged?.Invoke(this, EventArgs.Empty);
                    break;
            }
        }

        private void HandleGameBroadcast(string content)
        {
            if (string.IsNullOrEmpty(content))
                return;

            if (content.Contains("successfullyregistered") || content.Contains("successfullyquickstartregistered"))
            {
                _currentState = ASHandleState.Connected;
                StateChanged?.Invoke(this, EventArgs.Empty);
                OnRegistered();
            }

            MessageResieved?.Invoke(this, content);
        }

        private void OnBridgeConnectionLost()
        {
            Dispatch(() =>
            {
                lock (_lockObject)
                {
                    if (_currentState == ASHandleState.NotConnected)
                        return;

                    GamePID = 0;
                    IsValid = false;
                    _currentState = ASHandleState.NotConnected;
                    StateChanged?.Invoke(this, EventArgs.Empty);
                }
            });
        }

        private void OnRegistered()
        {
            Registered?.Invoke(this, EventArgs.Empty);
            for (int i = 0; i < _queuedCommands.Count; i++)
            {
                var command = _queuedCommands.Dequeue();
                _connection.Send(command);
                CommandSent?.Invoke(this, new CommandInfo(command, CommandInfo.CommandStatus.Sent));
            }
        }

        public void Dispose()
        {
            _connection.Dispose();
        }
    }
}
