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

        public WndProcMessageService()
        {
            targetWindowHandle = IntPtr.Zero;
        }

        public WndProcMessageService(IntPtr wHandle)
        {
            targetWindowHandle = wHandle;
        }

        public void Handle(IntPtr wHandle)
        {
            targetWindowHandle = wHandle;
        }

        public bool Handle(string wName)
        {
            var tempHwnd = WinAPI.FindWindow(null, wName);
            if (tempHwnd == null)
            {
                return false;
            }
            targetWindowHandle = tempHwnd;
            return true;
        }

        public void Command(uint msgType, string msgContent)
        {
            var cds = new COPYDATASTRUCT()
            {
                cbData = msgContent.Length + 1,
                lpData = msgContent
            };

            WinAPI.SendMessage(targetWindowHandle, msgType, IntPtr.Zero, ref cds);
        }

        protected override void WndProc(Message message)
        {
            if (message.Msg != 0x001C) //Ignore WM_ACTIVEAPP Message
                MessageRecieved?.Invoke(this, message);
            base.WndProc(message);
        }
    }
}
