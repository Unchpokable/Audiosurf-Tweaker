using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using TweakerScripts;

namespace SkinChangerRestyle.Core.ServerSwapper
{
    internal class ServerSwapper
    {
        public ServerSwapper()
        {
            _globalDefines = new Dictionary<string, string>()
            {
                {"%AS%", Directory.GetDirectoryRoot(Directory.GetDirectoryRoot(SettingsProvider.GameTexturesPath)) }, // => AS\engine\textures -> AS\
                {"%BACKUP_PATH%", Assembly.GetExecutingAssembly().Location + "\\Backups"},
            };
        }
         
        private Dictionary<string, string> _globalDefines;

        private string _packageRootCharacter = "%PACKAGE_ROOT%";

        public void SwapServer(string packagePath)
        {
            if (!Directory.Exists(packagePath))
                throw new ArgumentException("Given directory is not exists");

            var currentlyInstalledServer = SettingsProvider.InstalledServerPackageName;
            if (!currentlyInstalledServer.Equals(SettingsProvider.DefaultDylanServerName))
            {
                var oldPackage = SettingsProvider.BaseServerPackagePath + currentlyInstalledServer; // Servers\Wavebreaker\
                var oldPackageScript = $"{oldPackage}\\{currentlyInstalledServer}-Package.s.tws";
                RemoveServer(oldPackageScript, new Dictionary<string, string>() { { _packageRootCharacter, oldPackage } });
            }

            var newScript = $"{packagePath}\\{packagePath}-Package.s.tws";
            InstallServer(newScript, new Dictionary<string, string>() { { _packageRootCharacter, packagePath } });
        }

        public async void InstallServer(string scriptFile, Dictionary<string, string> moreDefines = null)
        {
            await ExecuteServerPackageScript(scriptFile, "INSTALL", moreDefines);
        }

        public async void RemoveServer(string scriptFile, Dictionary<string, string> moreDefines = null)
        {
            await ExecuteServerPackageScript(scriptFile, "REMOVE", moreDefines);
        }

        private Task ExecuteServerPackageScript(string file, string section, Dictionary<string, string> moreDefines = null)
        {
            var parser = new ScriptParser(_globalDefines);
            if (moreDefines != null)
            {
                foreach (var def in moreDefines)
                {
                    parser.DefinedCharacters.Add(def.Key, def.Value);
                }
            }

            var script = parser.ParseScriptFromFile(file);
            if (!script.ContainsKey(section))
                throw new ArgumentException($"Requiered section {section} not found in script file: {file}");

            return Task.Run(() => { script[section].Execute(); });
        }
    }
}