using System;
using ASCommander;
using ASCommander.PInvoke;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Forms;
using System.Windows.Media;
using System.Threading;
using System.Threading.Tasks;
using System.Diagnostics;

namespace SkinChangerRestyle.MVVM.Model
{
    internal delegate void MessageEventHandler(object sender, string messageContent);

    class AudiosurfHandle
    {
        private AudiosurfHandle()
        {
            _wndProcMessageService = new WndProcMessageService();
            _currentState = ASHandleState.NotConnected;
            StateChanged?.Invoke(this, EventArgs.Empty);
            _wndProcMessageService.MessageRecieved += OnMessageRecieved;

            _timer = new System.Windows.Forms.Timer();
            _timer.Interval = 1000;
            _timer.Tick += (s, e) =>
            {
                if (_currentState == ASHandleState.Awaiting)
                {
                    TryConnect();
                    return;
                }

                if (Handle == IntPtr.Zero)
                {
                    TryConnect();
                    return;
                }
                ValidateHandle();
            };

            _timer.Start();
            _queuedCommands = new Queue<string>();
        }

        public event EventHandler StateChanged;
        public event EventHandler Registered;
        public event MessageEventHandler MessageResieved;
        public bool IsValid { get; private set; }
        public IntPtr Handle { get; private set; }
        public string StateMessage => _currentState.Message;
        public SolidColorBrush StateColor => _currentState.ColorInterpretation;

        private System.Windows.Forms.Timer _timer;
        private Queue<string> _queuedCommands;
        private WndProcMessageService _wndProcMessageService;
        private object _lockObject = new object();

        private ASHandleState _currentState;
        private static AudiosurfHandle _instance;

        public static AudiosurfHandle Instance
        {
            get
            {
                if (_instance != null) return _instance;
                _instance = new AudiosurfHandle();
                return _instance;
            }
        }

        public bool TryConnect()
        {
            lock (_lockObject)
            {
                var handle = GetAudiosurfMainwindowHandle();
                return SetHandle(handle);
            }
        }

        public bool SetHandle(Process target)
        {
            lock (_lockObject)
            {
                return SetHandle(target.MainWindowHandle);
            }
        }

        public void Command(string message)
        {
            if (_wndProcMessageService.Valid == false)
            {
                _queuedCommands.Enqueue(message);
                return;
            }

            _wndProcMessageService.Command(WinAPI.WM_COPYDATA, message);
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
                        if (cds.lpData.Contains("successfullyregistered"))
                        {
                            _currentState = ASHandleState.Connected;
                            StateChanged?.Invoke(this, EventArgs.Empty);
                            OnRegistered();
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
            }
        }

        private bool SetHandle(IntPtr handle)
        {
            if (handle == IntPtr.Zero) return false;
            Handle = handle;
            _currentState = ASHandleState.Awaiting;
            StateChanged?.Invoke(this, EventArgs.Empty);
            _wndProcMessageService.Handle(handle);
            _wndProcMessageService.Command(WinAPI.WM_COPYDATA, "ascommand registerlistenerwindow AsMsgHandler");
            IsValid = true;
            return true;
        }

        private IntPtr GetAudiosurfMainwindowHandle()
        {
            var processes = Process.GetProcessesByName("QuestViewer");
            if (processes.Length == 0) return IntPtr.Zero;
            return processes[0].MainWindowHandle;
        }

        public class ASHandleState
        {
            public string Message { get; set; }
            public SolidColorBrush ColorInterpretation { get; set; }

            private static string asNotConnectedStatusColor = "#ff0000";
            private static string asConnectedStatusColor = "#11ff00";
            private static string asWaitForRegistratingColor = "#ffff00";

            private static ASHandleState connectedState;
            private static ASHandleState authorizationAwaitingState;
            private static ASHandleState notConnectedState;

            private ASHandleState(string message, string hexColor)
            {
                Message = message;
                ColorInterpretation = (SolidColorBrush)new BrushConverter().ConvertFromString(hexColor);
            }

            public static ASHandleState Connected
            {
                get
                {
                    if (connectedState != null) return connectedState;
                    connectedState = new ASHandleState("Audiosurf connected", asConnectedStatusColor);
                    return connectedState;
                }
            }

            public static ASHandleState Awaiting
            {
                get
                {
                    if (authorizationAwaitingState != null) return authorizationAwaitingState;
                    authorizationAwaitingState = new ASHandleState("Handled. Wait for AS approve", asWaitForRegistratingColor);
                    return authorizationAwaitingState;
                }
            }

            public static ASHandleState NotConnected
            {
                get
                {
                    if (notConnectedState != null) return notConnectedState;
                    notConnectedState = new ASHandleState("Audiosurf not connected", asNotConnectedStatusColor);
                    return notConnectedState;
                }
            }
        }
    }
}
