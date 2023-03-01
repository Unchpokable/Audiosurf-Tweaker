using System;
using System.IO;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using ASCommander;
using SkinChangerRestyle.Core.Extensions;

namespace SkinChangerRestyle.Core
{
    internal class OverlayHelper
    {
        static OverlayHelper()
        {
            _this = new OverlayHelper();
        }

        private OverlayHelper() { }

        public event EventHandler OverlayInjected;

        private static OverlayHelper _this;
        private string _defaultInjectorPath = "\\Plugins\\InjectHelper.exe";
        private string _defaultD3DOverlayPlugin = "\\Plugins\\InternalOverlayRenderer.dll";

        public static OverlayHelper Instance
        {
            get
            {
                if (_this == null)
                    _this = new OverlayHelper();
                return _this;
            }
        }

        public async void InjectOverlayPlugin()
        {
            string currentPath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            var overlayPluginPath = currentPath + _defaultD3DOverlayPlugin;
            AudiosurfHandle.Instance.MessageResieved += OnPulse;

            Extensions.Extensions.Cmd($"cd /d {currentPath}\\Plugins && InjectHelper.exe {AudiosurfHandle.Instance.GamePID} \"{overlayPluginPath}\"");

            await Task.Delay(500);

            await Task.Delay(50);
            AudiosurfHandle.Instance.Command($"tw-update-listener {AudiosurfHandle.Instance.ListenerWindowCaption}");
            await Task.Delay(200);
            AudiosurfHandle.Instance.Command("tw-pulse");
            AudiosurfHandle.Instance.MessageServiceInitialized += (s, e) => AudiosurfHandle.Instance.Command($"tw-update-listener {AudiosurfHandle.Instance.ListenerWindowCaption}");
        }

        public void UpdateOverlayData(string commandHeader, string content)
        {
            if (!SettingsProvider.IsOverlayInstanceAlive)
                return;

            AudiosurfHandle.Instance.Command(commandHeader + " " + content);
        }

        private void OnPulse(object sender, string content)
        {
            if (content.Contains("tw-responce ok")) 
            {
                SettingsProvider.IsOverlayInstanceAlive = true; 
                AudiosurfHandle.Instance.MessageResieved -= OnPulse;
                OverlayInjected?.Invoke(this, EventArgs.Empty);
                AudiosurfHandle.Instance.Command(
 $@"tw-config font-color {SettingsProvider.InfopanelFontColor}; font-size {SettingsProvider.InfopanelFontSize}; infopanel-xoffset {SettingsProvider.InfopanelXOffset}; infopanel-yoffset {SettingsProvider.InfopanelYOffset}");
            }
        }
    }
}
