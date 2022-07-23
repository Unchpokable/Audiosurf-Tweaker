using ASCommander.PInvoke;
using System;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace ASCommander
{
    public class WndProcMessageService : WinApiServiceBase
    {
        public event EventHandler<Message> MessageRecieved;
        public Func<string, bool> MsgContentFilter { get; set; }

        private IntPtr targetWindowHandle;
        private bool isValid;

        public WndProcMessageService()
        {
            targetWindowHandle = IntPtr.Zero;
            isValid = false;
        }

        public WndProcMessageService(IntPtr wHandle)
        {
            targetWindowHandle = wHandle;
            isValid = true;
        }

        public void Handle(IntPtr wHandle)
        {
            targetWindowHandle = wHandle;
            isValid = true;
        }

        public bool Handle(string wName)
        {
            var tempHwnd = WinAPI.FindWindow(null, wName);
            if (tempHwnd == null)
            {
                isValid = false;
                return false;
            }
            targetWindowHandle = tempHwnd;
            isValid = true;
            return true;
        }

        public void Command(uint msgType, string msgContent)
        {
            if (!isValid)
                return;

            var cds = new COPYDATASTRUCT()
            {
                cbData = msgContent.Length + 1,
                lpData = msgContent
            };

            WinAPI.SendMessage(targetWindowHandle, msgType, IntPtr.Zero, ref cds);
        }

        public void Invalidate()
        {
            targetWindowHandle = IntPtr.Zero;
            isValid = false;
        }

        protected override void WndProc(Message message)
        {
            if (message.Msg != WinAPI.WM_ACTIVATEAPP) //Ignore WM_ACTIVEAPP Message cause it useless for this app
                MessageRecieved?.Invoke(this, message);
            base.WndProc(message);
        }
    }
}
