using System;
using ASCommander;
using ASCommander.PInvoke;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Forms;
using System.Windows.Media;
using System.Threading;
using System.Threading.Tasks;

namespace SkinChangerRestyle.MVVM.Model
{
    internal delegate void MessageEventHandler(object sender, string messageContent);

    class AudiosurfHandle
    {
        public event EventHandler StateChanged;
        public event EventHandler Registered;
        public event MessageEventHandler MessageResieved;
        public bool IsValid { get; private set; }
        public IntPtr Handle { get; private set; }
        public string StateMessage => currentState.Message;
        public SolidColorBrush StateColor => currentState.ColorInterpretation;

        private System.Windows.Forms.Timer _timer;

        public static AudiosurfHandle Instance
        {
            get
            {
                if (instance != null) return instance;
                instance = new AudiosurfHandle();
                return instance;
            }
        }

        private WndProcMessageService wndProcMessageService;
        private object lockObject = new object();
        
        private ASHandleState currentState;
        private static AudiosurfHandle instance;

        private AudiosurfHandle()
        {
            wndProcMessageService = new WndProcMessageService();
            currentState = ASHandleState.NotConnected;
            StateChanged?.Invoke(this, EventArgs.Empty);
            wndProcMessageService.MessageRecieved += OnMessageRecieved;

            _timer = new System.Windows.Forms.Timer();
            _timer.Interval = 1000;
            _timer.Tick += (s, e) =>
            {
                if (currentState == ASHandleState.Awaiting)
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
        }


        public bool TryConnect()
        {
            lock (lockObject)
            {
                var handle = WinAPI.FindWindow(null, "Audiosurf");
                if (handle == IntPtr.Zero)
                    return false;
                Handle = handle;
                currentState = ASHandleState.Awaiting;
                StateChanged?.Invoke(this, EventArgs.Empty);
                wndProcMessageService.Handle(handle);
                wndProcMessageService.Command(WinAPI.WM_COPYDATA, "ascommand registerlistenerwindow AsMsgHandler");
                IsValid = true;
                return true;
            }
        }

        public void Command(string message)
        {
            wndProcMessageService.Command(WinAPI.WM_COPYDATA, message);
        }

        public void OnMessageRecieved(object sender, Message message)
        {
            lock (lockObject)
            {
                if (message.Msg == WinAPI.WM_COPYDATA) //Audiosurf handle object interested only in WM_COPYDATA messages
                {
                    var cds = (COPYDATASTRUCT)message.GetLParam(typeof(COPYDATASTRUCT));
                    if (cds.cbData > 0)
                    {
                        if (cds.lpData.Contains("successfullyregistered"))
                        {
                            currentState = ASHandleState.Connected;
                            StateChanged?.Invoke(this, EventArgs.Empty);
                            Registered?.Invoke(this, EventArgs.Empty);
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
                currentState = ASHandleState.NotConnected;
                wndProcMessageService.Invalidate();
                IsValid = false;
                StateChanged?.Invoke(this, EventArgs.Empty);
                return false;
            }
            return true;
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
