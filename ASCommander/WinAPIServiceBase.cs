using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ASCommander
{
    public abstract class WinApiServiceBase : IDisposable
    {
        protected static string nwCaption = "AsMsgHandler";

        private sealed class SpongeWindow : NativeWindow
        {
            public event EventHandler<Message> WndProced;

            private CreateParams windowParams;

            public SpongeWindow()
            {
                windowParams = new CreateParams();
                windowParams.Caption = nwCaption;
                CreateHandle(windowParams);
            }

            protected override void WndProc(ref Message m)
            {
                WndProced?.Invoke(this, m);
                base.WndProc(ref m);
            }
        }

        private static readonly SpongeWindow Sponge;
        protected static readonly IntPtr SpongeHandle;

        static WinApiServiceBase()
        {
            Sponge = new SpongeWindow();
            SpongeHandle = Sponge.Handle;
        }

        protected WinApiServiceBase()
        {
            Sponge.WndProced += LocalWndProced;
        }

        private void LocalWndProced(object sender, Message message)
        {
            WndProc(message);
        }

        protected virtual void WndProc(Message message)
        { }

        public virtual void Dispose()
        {
            Sponge.WndProced -= LocalWndProced;
        }
    }
}
