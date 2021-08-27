using ASCommander.PInvoke;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Win32;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace ASCommander
{
    class WndProcMessageService : WinApiServiceBase
    {
        public EventHandler<Message> OnMessageRecieved;
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
            switch (message.Msg)
            {
                case (int)WinAPI.WM_COPYDATA:
                    {
                        if(MsgContentFilter(Marshal.PtrToStringUni(message.LParam)))
                            OnMessageRecieved?.Invoke(this, message);
                        break;
                    }
            }
            base.WndProc(message);
        }
    }
}
