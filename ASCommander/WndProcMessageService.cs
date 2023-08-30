using ASCommander.PInvoke;
using System;
using System.Windows.Forms;

namespace ASCommander
{
    public class WndProcMessageService : WinApiServiceBase
    {
        public event EventHandler<Message> MessageRecieved;
        public Func<string, bool> MsgContentFilter { get; set; }
        public bool Valid => _isValid;

        private IntPtr _targetWindowHandle;
        private bool _isValid;

        public WndProcMessageService() : base()
        {
            _targetWindowHandle = IntPtr.Zero;
            _isValid = false;
        }

        public WndProcMessageService(IntPtr wHandle) : base()
        {
            _targetWindowHandle = wHandle;
            _isValid = true;
        }

        public void Handle(IntPtr wHandle)
        {
            _targetWindowHandle = wHandle;
            _isValid = true;
        }

        public bool Handle(string wName)
        {
            var tempHwnd = WinAPI.FindWindow(null, wName);
            if (tempHwnd == IntPtr.Zero)
            {
                _isValid = false;
                return false;
            }
            _targetWindowHandle = tempHwnd;
            _isValid = true;
            return true;
        }

        public void Command(uint msgType, string msgContent)
        {
            if (!_isValid)
                return;

            var cds = new COPYDATASTRUCT()
            {
                cbData = msgContent.Length + 1,
                lpData = msgContent
            };

            WinAPI.SendMessage(_targetWindowHandle, msgType, SpongeHandle, ref cds);
        }

        public void Invalidate()
        {
            _targetWindowHandle = IntPtr.Zero;
            _isValid = false;
        }

        protected override void WndProc(Message message)
        {
            if (message.Msg != WinAPI.WM_ACTIVATEAPP) //Ignore WM_ACTIVEAPP Message cause it useless for this app
                MessageRecieved?.Invoke(this, message);
            base.WndProc(message);
        }
    }
}
