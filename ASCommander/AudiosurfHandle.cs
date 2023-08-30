using System;
using ASCommander.PInvoke;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Threading.Tasks;
using System.Diagnostics;
using System.ComponentModel;

namespace ASCommander
{
    public delegate void MessageEventHandler(object sender, string messageContent);

    public class AudiosurfHandle : IDisposable
    {
        private AudiosurfHandle()
        {
            _wndProcMessageService = new WndProcMessageService();
            _currentState = ASHandleState.NotConnected;
            _wndProcMessageService.MessageRecieved += OnMessageRecieved;

            _timer = new Timer
            {
                Interval = 1000
            };

            _timer.Tick += (s, e) =>
            {
                if (_autoHandling)
                {
                    if (_currentState == ASHandleState.Awaiting)
                    {
                        if ((DateTime.Now - _lastConnectionRequestSended).TotalSeconds > _connectionTimeout)
                            TryConnect();
                        return;
                    }

                    if (Handle == IntPtr.Zero)
                    {
                        TryConnect();
                        return;
                    }
                    ValidateHandle();
                }
                else 
                    ValidateHandle();
            };

            _timer.Start();
            _queuedCommands = new Queue<string>();
        }

        public event EventHandler StateChanged;
        public event EventHandler Registered;
        public event MessageEventHandler MessageResieved;
        public event EventHandler<CommandInfo> CommandSent;
        public event EventHandler MessageServiceInitialized;

        public bool IsValid { get; private set; }
        public IntPtr Handle { get; private set; }
        public int GamePID { get; private set; }

        public string ListenerWindowCaption => WinApiServiceBase.ListenerWindowCaption;

        public string StateMessage => _currentState.Message;
        public string StateColor => _currentState.ColorInterpretation;

        private Timer _timer;
        private Queue<string> _queuedCommands;
        private WndProcMessageService _wndProcMessageService;
        private bool _autoHandling;
        private ASHandleState _currentState;
        private static AudiosurfHandle _instance;
        private DateTime _lastConnectionRequestSended;
        private double _connectionTimeout = 30f;

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

        public bool ReinitializeWndProcMessageService()
        {
            try
            {
                IsValid = false;
                Handle = IntPtr.Zero;
                _timer.Interval = 1000;
                _currentState = ASHandleState.NotConnected;
                StateChanged?.Invoke(this, EventArgs.Empty);
                _autoHandling = true;
                _wndProcMessageService = new WndProcMessageService();
                _wndProcMessageService.MessageRecieved += OnMessageRecieved;
                MessageServiceInitialized?.Invoke(this, EventArgs.Empty);
                return true;
            }
            catch { return false; }
        }

        public void StopAutoHandling()
        {
            _timer.Stop();
        }

        public void StartAutoHandling()
        {
            _timer.Start();
        }

        public bool TryConnect()
        {
            lock (_lockObject)
            {
                _lastConnectionRequestSended = DateTime.Now;
                var handle = GetAudiosurfMainwindowHandle(out bool shouldUseQuickRegister);
                return SetHandle(handle, shouldUseQuickRegister);
            }
        }

        public bool SetHandle(Process target)
        {
            lock (_lockObject)
            {
                return SetHandle(target.MainWindowHandle);
            }
        }

        private bool SetHandle(IntPtr handle, bool sendQuickRegisterCommand = false)
        {
            if (handle == IntPtr.Zero)
            {
                _currentState = ASHandleState.NotConnected;
                StateChanged?.Invoke(this, EventArgs.Empty);
                return false;
            }

            var registrationString = sendQuickRegisterCommand ? "quickstartregisterwindow" : "registerlistenerwindow";

            Handle = handle;
            _currentState = ASHandleState.Awaiting;
            StateChanged?.Invoke(this, EventArgs.Empty);
            _timer.Interval = 5000;
            _wndProcMessageService.Handle(handle);
            _wndProcMessageService.Command(WinAPI.WM_COPYDATA, $"ascommand {registrationString} {WinApiServiceBase.ListenerWindowCaption}");
            CommandSent?.Invoke(this, new CommandInfo($"ascommand {registrationString} {WinApiServiceBase.ListenerWindowCaption} to hwnd {handle}", 
                                CommandInfo.CommandStatus.Sent));
            IsValid = true;
            return true;
        }

        public void Command(string message)
        {
            lock (_lockObject)
            {
                if (_wndProcMessageService.Valid == false)
                {
                    if (message.Contains("reloadtextures")) return; //No need to enqueue reloadtextures command
                    _queuedCommands.Enqueue(message);
                    CommandSent?.Invoke(this, new CommandInfo(message, CommandInfo.CommandStatus.Enqueued));
                    return;
                }

                _wndProcMessageService.Command(WinAPI.WM_COPYDATA, message);
                CommandSent?.Invoke(this, new CommandInfo(message, CommandInfo.CommandStatus.Sent));
            }
        }

        public void OnMessageRecieved(object sender, Message message)
        {
            lock (_lockObject)
            {
                if (message.Msg == WinAPI.WM_COPYDATA) //Audiosurf handle object interested only in WM_COPYDATA messages
                {
                    var cds = (COPYDATASTRUCT)message.GetLParam(typeof(COPYDATASTRUCT));
                    if (cds.cbData > 0)
                    {
                        if (cds.lpData.Contains("successfullyregistered") || cds.lpData.Contains("successfullyquickstartregistered"))
                        {
                            _currentState = ASHandleState.Connected;
                            StateChanged?.Invoke(this, EventArgs.Empty);
                            OnRegistered();
                            StartAutoHandling();
                            _autoHandling = false;
                        }
                        MessageResieved?.Invoke(this, cds.lpData);
                    }
                }
            }
        }

        public bool ValidateHandle()
        {
            if (!WinAPI.IsWindow(Handle))
            {
                Handle = IntPtr.Zero;
                _currentState = ASHandleState.NotConnected;
                _wndProcMessageService.Invalidate();
                IsValid = false;
                StateChanged?.Invoke(this, EventArgs.Empty);
                _autoHandling = true;
                _timer.Interval = 1000;
                return false;
            }
            return true;
        }

        private void OnRegistered()
        {
            Registered?.Invoke(this, EventArgs.Empty);
            for (int i = 0; i < _queuedCommands.Count; i++)
            {
                var command = _queuedCommands.Dequeue();
                _wndProcMessageService.Command(WinAPI.WM_COPYDATA, command);
                CommandSent?.Invoke(this, new CommandInfo(command, CommandInfo.CommandStatus.Sent));
            }
        }

        private IntPtr GetAudiosurfMainwindowHandle(out bool shouldUseQuickRegister)
        {
            var processes = Process.GetProcessesByName("QuestViewer");
            TimeSpan runtime;

            if (processes.Length > 0)
                try
                {
                    runtime = DateTime.Now - processes[0].StartTime;
                }
                catch (Win32Exception ex)
                {
                    if (ex.NativeErrorCode != 5)
                        throw;
                    runtime = TimeSpan.FromHours(10000f);
                }
            else
                runtime = TimeSpan.FromHours(1000000f); // Just big timespan to set "quickstart" flag to false

            if (runtime.TotalSeconds < 30)
                shouldUseQuickRegister = true;
            else
                shouldUseQuickRegister = false;

            if (processes.Length == 0) return IntPtr.Zero;

            GamePID = processes[0].Id;
            return processes[0].MainWindowHandle;
        }

        public void Dispose()
        {
            _wndProcMessageService.Dispose();
        }
    }
}
