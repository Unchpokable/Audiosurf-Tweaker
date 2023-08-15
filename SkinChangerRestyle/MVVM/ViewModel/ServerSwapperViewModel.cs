using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.Core.ServerSwapper;
using SkinChangerRestyle.MVVM.Model;
using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;

namespace SkinChangerRestyle.MVVM.ViewModel
{
    internal class ServerSwapperViewModel : ObservableObject 
    {
        public ServerSwapperViewModel()
        {
            Servers = new ObservableCollection<ServerSwapCard>();
            if (!Directory.Exists(SettingsProvider.BaseServerPackagePath))
                Directory.CreateDirectory(SettingsProvider.BaseServerPackagePath);

            var packages = Directory.EnumerateDirectories(SettingsProvider.BaseServerPackagePath);

            foreach (var package in packages)
            {
                var card = new ServerSwapCard(Path.GetFullPath(package))
                {
                    ServerName = package.Split('\\').Last()
                };
                var specs = Directory.EnumerateFiles(package).ToList().Find(f => f.EndsWith(".specs"));
                if (specs != null)
                {
                    var specsList = File.ReadAllLines(specs).Select(x => x.Split('=')).ToDictionary(x => x[0], x => x[1]);
                    if (specsList != null && specsList.ContainsKey("remote"))
                    {
                        card.ServerHost = specsList["remote"];
                        card.SpecsServerRemote = specsList["remote"];
                    }
                }

                Servers.Add(card);

                InstallSelectedServer = new RelayCommand(InstallSelectedServerInternal);
                RemoveSelectedServer = new RelayCommand(RemoveSelectedServerInternal);
                SaveInstallerScript = new RelayCommand(SaveInstallerScriptInternal);
                DiscardScriptChanges = new RelayCommand(DiscardScriptChangesInternal);
                DefineNewName = new RelayCommand(DefineNewNameInternal);
                RemoveSelected = new RelayCommand(RemoveInterpreterDefine);

                InterpreterDefines = new ObservableCollection<ScriptInterpreterDefinedCharacter>()
                {
                    new ScriptInterpreterDefinedCharacter("%AS%", Path.GetDirectoryName(Path.GetDirectoryName(SettingsProvider.GameTexturesPath)) ) { Freezed = true }, // => AS\engine\textures -> AS\
                    new ScriptInterpreterDefinedCharacter("%BACKUP_PATH%", Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location) + "\\Backups") { Freezed = true },
                };

                NameDefinitionProxy = new ScriptInterpreterDefinedCharacter();

                Status = "Ready";
            }
        }

        public ServerSwapCard SelectedServer
        {
            get => _selectedServer;
            set
            {
                if (value == null)
                    throw new ArgumentNullException(nameof(value));
                _selectedServer = value;
                LoadPackageScriptAsync(value);
                OnPropertyChanged(nameof(SelectedServer));
            }
        }

        public string SelectedPackageScript
        {
            get => _selectedServerInstallScriptText;
            set
            {
                _selectedServerInstallScriptText = value;
                OnPropertyChanged(nameof(SelectedPackageScript));
            }
        }

        public string Status
        {
            get => _statusMessage;
            set
            {
                _statusMessage = value;
                OnPropertyChanged(nameof(Status));
            }
        }

        public ObservableCollection<ServerSwapCard> Servers { get; }

        public RelayCommand InstallSelectedServer { get; set; }
        public RelayCommand RemoveSelectedServer { get; set; }
        public RelayCommand SaveInstallerScript { get; set; }
        public RelayCommand DiscardScriptChanges { get; set; }
        public RelayCommand DefineNewName { get; set; }
        public RelayCommand RemoveSelected { get; set; }

        public ScriptInterpreterDefinedCharacter NameDefinitionProxy { get; set; }
        public ScriptInterpreterDefinedCharacter SelectedDefineItem { get; set; }
        public ObservableCollection<ScriptInterpreterDefinedCharacter> InterpreterDefines { get; set; }

        private string _selectedServerInstallScriptText;
        private ServerSwapCard _selectedServer;
        private string _statusMessage;

        private async void LoadPackageScriptAsync(ServerSwapCard server)
        {
            SelectedPackageScript = await Task.Run(() =>
            {
                var basePath = server.BasePackagePath;
                var scriptFile = Path.Combine(basePath, $"{server.ServerName}-Package.s.tws");
                var scriptText = File.ReadAllText(scriptFile);
                return scriptText;
            });
        }
        
        private async void InstallSelectedServerInternal(object o)
        {
            var path = string.Copy(SelectedServer.BasePackagePath);
            var packageName = string.Copy(SelectedServer.ServerName);
            var selectedCard = SelectedServer;

            await Task.Run(() =>
            {
                var swapper = new ServerSwapper(InterpreterDefines.ToDictionary(
                                                key => key.DefinedName,
                                                value => value.NameValue));

                swapper.SwapFailed += (s, e) =>
                {
                    ApplicationNotificationManager.Manager.ShowError("Fail!!", $"Swap Failure: {e.Message}");
                };

                swapper.SwapSuccessfull += (s, e) =>
                {
                    ApplicationNotificationManager.Manager.ShowSuccess("Done!", $"Server pacakge successfully installed");
                    ConfigurationManager.UpdateSection("InstalledServerPackageName", packageName);
                    SettingsProvider.InstalledServerPackageName = packageName;
                    selectedCard.Installed = true;
                };

                Status = "Installing Package :: Working...";
                swapper.SwapServer(path);
            })
            .ContinueWith((task) =>
            {
                UpdateStatusWithTaskResultAndNotify(task, false);
            });
        }

        private async void SaveInstallerScriptInternal(object o)
        {
            var text = string.Copy(_selectedServerInstallScriptText);
            var path = string.Copy(SelectedServer.BasePackagePath);
            var serverName = string.Copy(SelectedServer.ServerName);

            await Task.Run(() =>
            {
                Status = "Writing script text :: Working...";
                var scriptFile = Path.Combine(path, $"{serverName}-Package.s.tws");
                File.WriteAllText(scriptFile, text);
            })
                .ContinueWith((task) =>
            {
                UpdateStatusWithTaskResultAndNotify(task);
            });
        }

        private async void DiscardScriptChangesInternal(object o)
        {
            var path = string.Copy(SelectedServer.BasePackagePath);
            var serverName = string.Copy(SelectedServer.ServerName);

            await Task.Run(() =>
            {
                Status = "Restoring script text :: Working...";
                var scriptFile = Path.Combine(path, $"{serverName}-Package.s.tws");
                SelectedPackageScript = File.ReadAllText(scriptFile);
            })
                .ContinueWith((task) =>
            {
                UpdateStatusWithTaskResultAndNotify(task);
            });
        }

        private async void RemoveSelectedServerInternal(object obj)
        {
            var path = string.Copy(SelectedServer.BasePackagePath);
            var serverName = string.Copy(SelectedServer.ServerName);
            var selectedCard = SelectedServer;

            await Task.Run(() =>
            {
                var swapper = new ServerSwapper(InterpreterDefines.ToDictionary(
                                                key => key.DefinedName,
                                                value => value.NameValue));
                swapper.SwapFailed += (s, e) =>
                {
                    ApplicationNotificationManager.Manager.ShowError("Fail!!", $"Swap Failure: {e.Message}");
                };

                swapper.SwapSuccessfull += (s, e) =>
                {
                    ApplicationNotificationManager.Manager.ShowSuccess("Done!", $"Server pacakge successfully Removed");
                    ConfigurationManager.UpdateSection("InstalledServerPackageName", SettingsProvider.DefaultDylanServerName);
                    SettingsProvider.InstalledServerPackageName = SettingsProvider.DefaultDylanServerName;
                    selectedCard.Installed = false;
                };

                swapper.RemoveServerByPackage(path);
            })
            .ContinueWith((task) =>
            {
                    UpdateStatusWithTaskResultAndNotify(task, false);
            });
        }

        private void DefineNewNameInternal(object obj)
        {
            if (NameDefinitionProxy.DefinedName == "%PACKAGE_ROOT%")
            {
                ApplicationNotificationManager.Manager.ShowOverWindow("Restricted Action", "Unable to add name %PACKAGE_ROOT% - reserved name", Notification.Wpf.NotificationType.Warning);
                return;
            }
            var nameDefinitionProxyClone = NameDefinitionProxy.Clone();
            InterpreterDefines.Add(nameDefinitionProxyClone);
        }

        private void UpdateStatusWithTaskResultAndNotify(Task task, bool showSuccess = false)
        {
            if (task.IsFaulted || task.IsCanceled)
            {
                Status = "Operation Failed :: Ready";
                ApplicationNotificationManager.Manager.ShowError("Fail!", $"Installation Error - {task.Exception.Message}");
                return;
            }
            Status = "Operation Completed :: Ready";

            if (showSuccess) 
                ApplicationNotificationManager.Manager.ShowSuccess("Done!", $"Operation Completed");
        }

        private void RemoveInterpreterDefine(object o)
        {
            if (SelectedDefineItem == null)
                return;

            if (SelectedDefineItem.DefinedName.SameWith("%AS%", "%BACKUP_PATH%", "%PACKAGE_ROOT"))
            {
                ApplicationNotificationManager.Manager.ShowOverWindow("Restricted Action", "Requiered Interpreter name, can not remove", Notification.Wpf.NotificationType.Warning);
                return;
            }

            InterpreterDefines.Remove(SelectedDefineItem);
        }
    }
}
