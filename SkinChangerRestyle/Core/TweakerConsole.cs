using ASCommander;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SkinChangerRestyle.Core
{
    internal class TweakerConsole
    {
        public TweakerConsole()
        {
            _content = new StringBuilder();
            _asHandleInstance = AudiosurfHandle.Instance;
            _asHandleInstance.CommandSent += AppendInfo;
        }

        public event EventHandler ContentUpdated;

        public Action AttachedAction { get; set; }

        public void Flush()
        {
            _content.Clear();
            ContentUpdated?.Invoke(this, EventArgs.Empty);
            _content = new StringBuilder();
            Extensions.Extensions.DisposeAndClear();

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
            AttachedAction?.Invoke();
        }

        private StringBuilder _content;
        private AudiosurfHandle _asHandleInstance;
    }
}
