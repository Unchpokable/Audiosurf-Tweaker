using ASCommander;
using SkinChangerRestyle.Core.Extensions;
using System;
using System.Text;

namespace SkinChangerRestyle.MVVM.Model
{
    internal class TweakerConsole
    {
        public TweakerConsole()
        {
            _content = new StringBuilder();
            _asHandleInstance = AudiosurfHandle.Instance;
            _asHandleInstance.CommandSent += AppendInfo;
            _asHandleInstance.MessageResieved += AppendIncomingMessage;
        }

        public event EventHandler ContentUpdated;

        public Action AttachedAction { get; set; }

        public void Flush()
        {
            _content.Clear();
            ContentUpdated?.Invoke(this, EventArgs.Empty);
            _content = new StringBuilder();
            Extensions.DisposeAndClear();

        }

        public override string ToString()
        {
            return _content.ToString();
        }

        private void AppendInfo(object sender, CommandInfo info)
        {
            if (info == null)
                return;

            if (_content.Length >= (int)short.MaxValue)
            {
                Flush();
            }

            _content.Append($"[{DateTime.Now} :: INFO] Command {info.Status} - \"{info.CommandText}\"\n");
            ContentUpdated?.Invoke(this, EventArgs.Empty);
        }

        private void AppendIncomingMessage(object sender, string text)
        {
            if (string.IsNullOrEmpty(text)) return;

            if (_content.Length >= (int)short.MaxValue)
                Flush();

            _content.Append($"[{DateTime.Now}] :: INCOMING WNDPROC MSG] Message Recieved: \"{text}\"\n");

            ContentUpdated?.Invoke(this, EventArgs.Empty);
        }

        private StringBuilder _content;
        private AudiosurfHandle _asHandleInstance;
    }
}
