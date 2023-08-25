using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using ASCommander;

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

            var overlayAlreadyLoaded = await CheckIfOverlayDllAlreadyLoadedAsync();
            if (!overlayAlreadyLoaded)
                Extensions.Extensions.Cmd($"cd /d {currentPath}\\Plugins && InjectHelper.exe {AudiosurfHandle.Instance.GamePID} \"{overlayPluginPath}\"");

            await Task.Delay(500);

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

        private async Task<bool> CheckIfOverlayDllAlreadyLoadedAsync()
        {
            return await Task.Run(() =>
            {
                var targetProcessName = "QuestViewer";
                var processes = Process.GetProcessesByName(targetProcessName);

                if (processes.Length <= 0)
                    return false;

                foreach (var proc in processes)
                {
                    var modules = new IntPtr[1024];

                    if (EnumProcessModules(proc.Handle, modules, (uint)(modules.Length * IntPtr.Size), out uint cbNeeded))
                    {
                        for (var i = 0; i < cbNeeded; i++)
                        {
                            var builder = new StringBuilder(1024);

                            if (GetModuleFileNameEx(proc.Handle, modules[i], builder, (uint)builder.Capacity) > 0)
                            {
                                if (builder.ToString().ToLower().EndsWith(_defaultD3DOverlayPlugin.ToLower()))
                                {
                                    return true;
                                }
                            }
                        }
                    }
                }

                return false;
            });
        }

        [DllImport("psapi.dll")]
        public static extern bool EnumProcessModules(IntPtr hProcess, IntPtr[] lphModule, uint cb, out uint lpcbNeeded);

        [DllImport("psapi.dll", CharSet = CharSet.Unicode)]
        public static extern uint GetModuleFileNameEx(IntPtr hProcess, IntPtr hModule, [Out] StringBuilder lpBaseName, uint nSize);
    }
}
