using System;
using System.Text;
using AudiosurfInterface;
using TweakerUI.Core.Utils;

namespace TweakerUI.Models
{
    internal class TweakerConsole
    {
        public TweakerConsole()
        {
            _content = new StringBuilder();
            _asHandle = AudiosurfHandle.Instance;
            _asHandle.CommandSent += AppendInfo;
            _asHandle.MessageResieved += AppendIncomingMessage;
        }

        public event EventHandler ContentUpdated;

        public void Flush()
        {
            _content.Clear();
            ContentUpdated?.Invoke(this, EventArgs.Empty);
            Utils.DisposeAndClear();
        }

        public override string ToString() => _content.ToString();

        private void AppendInfo(object sender, CommandInfo info)
        {
            if (info == null)
                return;

            if (_content.Length >= short.MaxValue)
                Flush();

            _content.Append($"[{DateTime.Now} :: INFO] Command {info.Status} - \"{info.CommandText}\"\n");
            ContentUpdated?.Invoke(this, EventArgs.Empty);
        }

        private void AppendIncomingMessage(object sender, string text)
        {
            if (string.IsNullOrEmpty(text))
                return;

            if (_content.Length >= short.MaxValue)
                Flush();

            _content.Append($"[{DateTime.Now} :: INCOMING] Message received: \"{text}\"\n");
            ContentUpdated?.Invoke(this, EventArgs.Empty);
        }

        private readonly StringBuilder _content;
        private readonly AudiosurfHandle _asHandle;
    }
}
