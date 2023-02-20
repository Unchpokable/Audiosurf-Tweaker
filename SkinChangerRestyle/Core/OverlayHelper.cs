using System;
using System.IO;
using System.Reflection;
using System.Threading.Tasks;
using ASCommander;
using SkinChangerRestyle.Core.Extensions;

namespace SkinChangerRestyle.Core
{
    internal class OverlayHelper
    {

        public async void InjectOverlayPlugin()
        {
            string currentPath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            var overlayPluginPath = currentPath + _defaultD3DOverlayPlugin;

            Extensions.Extensions.Cmd($"cd /d {currentPath}\\Plugins && InjectHelper.exe {AudiosurfHandle.Instance.Handle} {overlayPluginPath}");

            await Task.Delay(100);

            AudiosurfHandle.Instance.MessageResieved += OnPulse;
            AudiosurfHandle.Instance.Command("tw-callback");
        }

        private void OnPulse(object sender, string content)
        {
            if (content.Contains("tw-responce ok")) 
            {
                SettingsProvider.IsOverlayInstanceAlive = true; 
                AudiosurfHandle.Instance.MessageResieved -= OnPulse;
                OverlayInjected?.Invoke(this, EventArgs.Empty);
            }
        }

        public event EventHandler OverlayInjected;

        private string _defaultInjectorPath = "\\Plugins\\InjectHelper.exe";
        private string _defaultD3DOverlayPlugin = "\\Plugins\\InternalOverlayRenderer.dll";
    }
}
