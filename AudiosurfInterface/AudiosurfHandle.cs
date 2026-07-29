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
            _queuedOverlayCommands = new Queue<string>();

            // The legacy WndProc listener delivered everything on the UI thread; the pipe pump is a
            // background thread. Capture the creating thread's context (the singleton is first
            // touched from the UI) so subscribers keep seeing events on the thread they always did.
            _syncContext = SynchronizationContext.Current;

            var caption = "AsMsgHandler_" + Convert.ToBase64String(Guid.NewGuid().ToByteArray()).Substring(0, 5);
            _connection = new AsBridgeConnection(caption);
            _connection.ReportReceived += OnReportReceived;
            _connection.ConnectionLost += OnBridgeConnectionLost;
            _connection.Diagnostic += OnConnectionDiagnostic;
            _connection.Start();
        }

        public event EventHandler StateChanged;
        public event EventHandler Registered;
        public event MessageEventHandler MessageResieved;
        public event MessageEventHandler OverlayMessageReceived;
        public event EventHandler<CommandInfo> CommandSent;
        public event EventHandler MessageServiceInitialized;
        public event Action<AsBridgeDiagnostic> Diagnostic;

        // Fired once the registration watchdog exhausts every recovery step (see RegistrationStage)
        // and settles into ASHandleState.CommunicationBroken. Carries a human-readable explanation so
        // a UI layer can surface it without knowing anything about the retry machinery.
        public event Action<string> CommunicationFailed;

        public bool IsValid { get; private set; }
        public int GamePID { get; private set; }

        public string ListenerWindowCaption => _connection.ListenerWindowCaption;

        public string StateMessage => _currentState.Message;
        public string StateColor => _currentState.ColorInterpretation;

        private AsBridgeConnection _connection;
        private ASHandleState _currentState;
        private readonly Queue<string> _queuedCommands;
        private readonly Queue<string> _queuedOverlayCommands;
        private readonly SynchronizationContext _syncContext;
        private static readonly Lazy<AudiosurfHandle> _lazyInstance =
            new Lazy<AudiosurfHandle>(() => new AudiosurfHandle(), LazyThreadSafetyMode.ExecutionAndPublication);

        private readonly object _lockObject = new object();

        // Registration-ack watchdog: asbridge picks quickstartregisterwindow vs registerlistenerwindow
        // by process age alone (ASBridge/src/service/service.cxx), which can guess wrong on a fast
        // machine where the game blows through the quickstart window before the bridge's 30ms
        // liveness poll even notices the window. Rather than trust that single guess, every attempt
        // gets a bounded wait for the game's "successfullyregistered"/"successfullyquickstartregistered"
        // broadcast before escalating - see RegistrationStage.
        private enum RegistrationStage
        {
            Idle,
            WaitingInitial,
            WaitingFallback,
            WaitingAfterRestart
        }

        private const int RegistrationAckTimeoutMs = 5000;

        private RegistrationStage _registrationStage = RegistrationStage.Idle;
        private bool _forceNormalRegistrationOnNextWindow;
        private bool _registrationGivenUp;
        private Timer _registrationTimeoutTimer;

        public static AudiosurfHandle Instance => _lazyInstance.Value;

        /// <summary>
        /// Tears down and recreates the bridge subprocess + pipe. Historically this recreated the
        /// WndProc listener window; the semantic - "reset the whole IPC channel" - is unchanged.
        /// </summary>
        public bool ReinitializeWndProcMessageService()
        {
            lock (_lockObject)
            {
                // A manual/full reinitialize is always a clean slate for the registration watchdog too -
                // unlike the internal auto-restart in HandleRegistrationTimeout (WaitingFallback), which
                // deliberately keeps _forceNormalRegistrationOnNextWindow/the WaitingAfterRestart timer
                // alive across the swap by calling ReinitializeConnectionLocked directly instead of this.
                CancelRegistrationWatchdogLocked();
                _registrationStage = RegistrationStage.Idle;
                _forceNormalRegistrationOnNextWindow = false;
                _registrationGivenUp = false;

                return ReinitializeConnectionLocked();
            }
        }

        // Swaps in a fresh AsBridgeConnection (new subprocess, new listener window caption). Does not
        // touch registration-watchdog state - callers decide whether this is a full reset (public
        // ReinitializeWndProcMessageService) or a mid-retry restart that must preserve it
        // (HandleRegistrationTimeout's WaitingFallback case). Must be called with _lockObject held.
        private bool ReinitializeConnectionLocked()
        {
            try
            {
                IsValid = false;
                GamePID = 0;
                _currentState = ASHandleState.NotConnected;
                StateChanged?.Invoke(this, EventArgs.Empty);

                // Build and start the replacement fully before touching the field: if construction
                // or Start() throws, _connection must still be left pointing at a live object
                // (the old one) rather than a disposed/half-wired one.
                var caption = "AsMsgHandler_" + Convert.ToBase64String(Guid.NewGuid().ToByteArray()).Substring(0, 5);
                var next = new AsBridgeConnection(caption);
                next.ReportReceived += OnReportReceived;
                next.ConnectionLost += OnBridgeConnectionLost;
                next.Diagnostic += OnConnectionDiagnostic;
                next.Start();

                var previous = _connection;
                _connection = next;

                previous.ReportReceived -= OnReportReceived;
                previous.ConnectionLost -= OnBridgeConnectionLost;
                previous.Diagnostic -= OnConnectionDiagnostic;
                previous.Dispose();

                MessageServiceInitialized?.Invoke(this, EventArgs.Empty);
                return true;
            }
            catch (Exception ex)
            {
                Diagnostic?.Invoke(new AsBridgeDiagnostic(AsBridgeDiagnosticLevel.Error, "Reinitialize", ex.Message, ex));
                return false;
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

        private static readonly string ReloadTexturesCommand = GameProtocol.Command(GameProtocol.ReloadTextures);

        public void Command(string message)
        {
            lock (_lockObject)
            {
                if (_currentState != ASHandleState.Connected)
                {
                    if (message == ReloadTexturesCommand) return; //No need to enqueue reloadtextures command
                    _queuedCommands.Enqueue(message);
                    CommandSent?.Invoke(this, new CommandInfo(message, CommandInfo.CommandStatus.Enqueued));
                    return;
                }

                var delivered = _connection.Send(message);
                CommandSent?.Invoke(this,
                    new CommandInfo(message, delivered ? CommandInfo.CommandStatus.Sent : CommandInfo.CommandStatus.Enqueued));
                if (!delivered && message != ReloadTexturesCommand)
                    _queuedCommands.Enqueue(message);
            }
        }

        public void OverlayCommand(string overlay_payload)
        {
            lock (_lockObject)
            {
                if (_currentState != ASHandleState.Connected)
                {
                    _queuedOverlayCommands.Enqueue(overlay_payload);
                    return;
                }

                var delivered = _connection.SendOverlay(overlay_payload);
                if (!delivered)
                    _queuedOverlayCommands.Enqueue(overlay_payload);
            }
        }

        private void OnReportReceived(AsBridgeReport report)
        {
            Dispatch(() => HandleReport(report));
        }

        private void OnConnectionDiagnostic(AsBridgeDiagnostic diagnostic)
        {
            Diagnostic?.Invoke(diagnostic);
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

                    case AsBridgeReportType.OverlayForward:
                        HandleOverlayBroadcast(report.Details.Count > 0 ? report.Details[0] : string.Empty);
                        break;

                    case AsBridgeReportType.OverlayOk:
                    case AsBridgeReportType.OverlayFailed:
                        // Per-overlay acks; no-op for now.
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

                    if (_currentState != ASHandleState.Connected)
                        BeginRegistrationCycleLocked();
                    break;

                case AsBridgeProtocol.ServiceStatusWindowLost:
                    GamePID = 0;
                    IsValid = false;
                    _currentState = ASHandleState.NotConnected;
                    CancelRegistrationWatchdogLocked();
                    _registrationStage = RegistrationStage.Idle;
                    _forceNormalRegistrationOnNextWindow = false;
                    // The game process that never acked is gone - a freshly (re)started game deserves
                    // a clean shot at registration rather than staying latched in the broken state.
                    _registrationGivenUp = false;
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
                CancelRegistrationWatchdogLocked();
                _registrationStage = RegistrationStage.Idle;
                _forceNormalRegistrationOnNextWindow = false;
                _registrationGivenUp = false;

                _currentState = ASHandleState.Connected;
                StateChanged?.Invoke(this, EventArgs.Empty);
                OnRegistered();
            }

            MessageResieved?.Invoke(this, content);
        }

        private void HandleOverlayBroadcast(string content)
        {
            if (string.IsNullOrEmpty(content))
                return;

            OverlayMessageReceived?.Invoke(this, content);
        }

        private void OnBridgeConnectionLost()
        {
            Dispatch(() =>
            {
                lock (_lockObject)
                {
                    CancelRegistrationWatchdogLocked();
                    _registrationStage = RegistrationStage.Idle;
                    _forceNormalRegistrationOnNextWindow = false;
                    _registrationGivenUp = false;

                    if (_currentState == ASHandleState.NotConnected)
                        return;

                    GamePID = 0;
                    IsValid = false;
                    _currentState = ASHandleState.NotConnected;
                    StateChanged?.Invoke(this, EventArgs.Empty);
                }
            });
        }

        // Starts (or restarts, on the forced-normal path after a bridge restart) the wait for the
        // game's registration ack. Must be called with _lockObject already held.
        private void BeginRegistrationCycleLocked()
        {
            if (_registrationGivenUp)
                return;

            if (_forceNormalRegistrationOnNextWindow)
            {
                // The bridge's own automatic attempt already failed once on this game process (see
                // WaitingFallback below) - skip trusting its quickstart/normal guess a second time and
                // send the plain command ourselves as soon as the restarted bridge finds the window.
                _forceNormalRegistrationOnNextWindow = false;
                SendPlainRegisterListenerWindowLocked();
                BeginRegistrationWatchdogLocked(RegistrationStage.WaitingAfterRestart);
            }
            else
            {
                BeginRegistrationWatchdogLocked(RegistrationStage.WaitingInitial);
            }
        }

        private void BeginRegistrationWatchdogLocked(RegistrationStage stage)
        {
            _registrationStage = stage;
            _registrationTimeoutTimer?.Dispose();
            _registrationTimeoutTimer = new Timer(_ => Dispatch(HandleRegistrationTimeout), null, RegistrationAckTimeoutMs, Timeout.Infinite);
        }

        private void CancelRegistrationWatchdogLocked()
        {
            _registrationTimeoutTimer?.Dispose();
            _registrationTimeoutTimer = null;
        }

        private void SendPlainRegisterListenerWindowLocked()
        {
            var command = GameProtocol.Command($"{GameProtocol.RegisterListenerWindow} {_connection.ListenerWindowCaption}");
            var delivered = _connection.Send(command);
            CommandSent?.Invoke(this,
                new CommandInfo(command, delivered ? CommandInfo.CommandStatus.Sent : CommandInfo.CommandStatus.Enqueued));
        }

        // Escalation ladder for a registration attempt the game never acked:
        //   WaitingInitial      -> the bridge's own quickstart-or-normal guess got no reply; retry
        //                          with the plain command on the same bridge/window.
        //   WaitingFallback     -> the plain retry also got no reply; restart asbridge.exe under a
        //                          new listener window caption and force a plain retry the instant it
        //                          finds the game window again (BeginRegistrationCycleLocked above).
        //   WaitingAfterRestart -> even that got no reply (or the restart itself never produced a
        //                          window) - give up and tell the caller.
        private void HandleRegistrationTimeout()
        {
            lock (_lockObject)
            {
                if (_currentState == ASHandleState.Connected || _registrationGivenUp)
                    return;

                switch (_registrationStage)
                {
                    case RegistrationStage.WaitingInitial:
                        Diagnostic?.Invoke(new AsBridgeDiagnostic(AsBridgeDiagnosticLevel.Warning, "Registration",
                            "no ack for the bridge's own registration attempt within timeout, retrying with plain registerlistenerwindow"));
                        SendPlainRegisterListenerWindowLocked();
                        BeginRegistrationWatchdogLocked(RegistrationStage.WaitingFallback);
                        break;

                    case RegistrationStage.WaitingFallback:
                        Diagnostic?.Invoke(new AsBridgeDiagnostic(AsBridgeDiagnosticLevel.Warning, "Registration",
                            "fallback registration unanswered, restarting asbridge.exe under a new listener window"));
                        _forceNormalRegistrationOnNextWindow = true;
                        // Safety net covering the whole restart: it also has to fire if the restart
                        // fails outright or the new bridge never finds the window at all, neither of
                        // which produces a WINDOW_FOUND to hang the retry off of.
                        BeginRegistrationWatchdogLocked(RegistrationStage.WaitingAfterRestart);
                        // Disposing the old connection blocks on a subprocess kill + pipe pump join (up
                        // to ~2s) - run it off this callback so the watchdog timer isn't held. Goes
                        // through ReinitializeConnectionLocked (not the public
                        // ReinitializeWndProcMessageService) so the _forceNormalRegistrationOnNextWindow
                        // flag and the timer just started above survive the swap.
                        System.Threading.Tasks.Task.Run(() =>
                        {
                            lock (_lockObject)
                            {
                                ReinitializeConnectionLocked();
                            }
                        });
                        break;

                    case RegistrationStage.WaitingAfterRestart:
                        GiveUpRegistrationLocked();
                        break;
                }
            }
        }

        private void GiveUpRegistrationLocked()
        {
            _registrationGivenUp = true;
            _registrationStage = RegistrationStage.Idle;
            _forceNormalRegistrationOnNextWindow = false;
            CancelRegistrationWatchdogLocked();

            IsValid = false;
            _currentState = ASHandleState.CommunicationBroken;
            StateChanged?.Invoke(this, EventArgs.Empty);

            const string message =
                "Audiosurf never confirmed the listener registration, even after a retry and a bridge restart. " +
                "Restart Audiosurf Tweaker, the game, or your PC, then try again.";
            Diagnostic?.Invoke(new AsBridgeDiagnostic(AsBridgeDiagnosticLevel.Error, "Registration", message));
            CommunicationFailed?.Invoke(message);
        }

        private void OnRegistered()
        {
            Registered?.Invoke(this, EventArgs.Empty);
            while (_queuedCommands.Count > 0)
            {
                var command = _queuedCommands.Dequeue();
                var delivered = _connection.Send(command);
                CommandSent?.Invoke(this,
                    new CommandInfo(command, delivered ? CommandInfo.CommandStatus.Sent : CommandInfo.CommandStatus.Enqueued));
                if (!delivered)
                {
                    // Pipe is presumably dead - put it back and stop draining; the next successful
                    // registration will retry the whole queue rather than losing the rest silently.
                    _queuedCommands.Enqueue(command);
                    Diagnostic?.Invoke(new AsBridgeDiagnostic(AsBridgeDiagnosticLevel.Warning, "OnRegistered",
                        "queued command flush failed, pipe likely dropped"));
                    break;
                }
            }

            while (_queuedOverlayCommands.Count > 0)
            {
                var overlayPayload = _queuedOverlayCommands.Dequeue();
                if (!_connection.SendOverlay(overlayPayload))
                {
                    _queuedOverlayCommands.Enqueue(overlayPayload);
                    Diagnostic?.Invoke(new AsBridgeDiagnostic(AsBridgeDiagnosticLevel.Warning, "OnRegistered",
                        "queued overlay command flush failed, pipe likely dropped"));
                    break;
                }
            }
        }

        public void Dispose()
        {
            _registrationTimeoutTimer?.Dispose();
            _connection.Dispose();
        }
    }
}
