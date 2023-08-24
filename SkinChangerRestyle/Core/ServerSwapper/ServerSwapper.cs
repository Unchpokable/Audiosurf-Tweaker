using SkinChangerRestyle.Core.Extensions;
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using TweakerScripts;

namespace SkinChangerRestyle.Core.ServerSwapper
{
    internal class ServerSwapper
    {
        public ServerSwapper()
        {
            _globalDefines = new Dictionary<string, string>()
            {
                {"%AS%", Path.GetDirectoryName(Path.GetDirectoryName(SettingsProvider.GameTexturesPath)) }, // => AS\engine\textures -> AS\
                {"%BACKUP_PATH%", Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location) + "\\Backups"},
            };
        }

        public ServerSwapper(Dictionary<string, string> globalDefines)
        {
            _globalDefines = new Dictionary<string, string>(globalDefines);
        }

        public event EventHandler<Exception> SwapFailed;
        public event EventHandler SwapSuccessfull;

        private Dictionary<string, string> _globalDefines;

        private string _packageRootCharacter = "%PACKAGE_ROOT%";

        public void SwapServer(string packagePath, Dictionary<string, string> moreDefines = null)
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
            var packageName = new DirectoryInfo(packagePath).Name;

            var newScript = $"{packagePath}\\{packageName}-Package.s.tws";
            InstallServer(newScript, new Dictionary<string, string>() { { _packageRootCharacter, packagePath } }
                                         .MergedWith(moreDefines));
        }

        public void RemoveServerByPackage(string packagePath, Dictionary<string, string> moreDefines = null)
        {
            if (!Directory.Exists(packagePath))
                throw new ArgumentException("Given directory is not exists");

            var packageName = new DirectoryInfo(packagePath).Name;

            var script = Path.Combine(packagePath, $"{packageName}-Package.s.tws");
            RemoveServer(script, new Dictionary<string, string>() { { _packageRootCharacter, packagePath } }
                                     .MergedWith(moreDefines));
        }

        private void InstallServer(string scriptFile, Dictionary<string, string> moreDefines = null)
        {
            ExecuteServerPackageScript(scriptFile, "INSTALL", moreDefines);
        }

        private void RemoveServer(string scriptFile, Dictionary<string, string> moreDefines = null)
        {
            ExecuteServerPackageScript(scriptFile, "REMOVE", moreDefines);
        }

        private void ExecuteServerPackageScript(string file, string section, Dictionary<string, string> moreDefines = null)
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

            script[section].ScriptExecutionFault += (s ,e) =>
            {
                SwapFailed?.Invoke(this, e);
            };

            script[section].ScriptExecutionSuccess += (s, e) =>
            {
                SwapSuccessfull?.Invoke(this, e);
            };

            script[section].Execute();
        }
    }
}