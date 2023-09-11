using ChangerAPI.Utilities;
using Notification.Wpf;
using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.Core.ServerSwapper;
using SkinChangerRestyle.MVVM.Model;
using SkinChangerRestyle.Properties;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Forms;
using System.Windows.Media;
using SkinChangerRestyle.Core.Utils;
using DataFormats = System.Windows.DataFormats;
using Extensions = SkinChangerRestyle.Core.Extensions.Extensions;
using MessageBox = System.Windows.MessageBox;

namespace SkinChangerRestyle.MVVM.ViewModel
{
    internal class ServerSwapperViewModel : ObservableObject 
    {
        public ServerSwapperViewModel()
        {
            RefreshIcon = Resources.wifi.ToImageSource();
            ReloadIcon = Resources.refreshing.ToImageSource();

            Servers = new ObservableCollection<ServerSwapCard>();
            if (!Directory.Exists(SettingsProvider.BaseServerPackagePath))
                Directory.CreateDirectory(SettingsProvider.BaseServerPackagePath);

            LoadServers();

            InstallSelectedServer = new RelayCommand(InstallSelectedServerInternal);
            RemoveSelectedServer = new RelayCommand(RemoveSelectedServerInternal);
            SaveInstallerScript = new RelayCommand(SaveInstallerScriptInternal);
            DiscardScriptChanges = new RelayCommand(DiscardScriptChangesInternal);
            DefineNewVariable = new RelayCommand(DefineNewNameInternal);
            RemoveSelected = new RelayCommand(RemoveInterpreterDefine);
            RemoveServerPackage = new RelayCommand(RemoveServerPackageInternal);

            UpdateServersList = new RelayCommand(LoadServersCommand);
            UpdateServersNetworkState = new RelayCommand(UpdateServersNetStatsInternal);

            OpenGuidePage = new RelayCommand(o => GuidePageHelper.ShowServerSwapperGuile());

            InterpreterVariables = new ObservableCollection<InterpreterVariable>
            {
                new InterpreterVariable("%AS%", Path.GetDirectoryName(Path.GetDirectoryName(SettingsProvider.GameTexturesPath)) ) { Freezed = true }, // => AS\engine\textures -> AS\
                new InterpreterVariable("%BACKUP_PATH%", Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location) + "\\Backups") { Freezed = true },
            };

            VariableDefinitionProxy = new InterpreterVariable();

            Status = "Ready";
            Ready = true;
        }

        

        public ServerSwapCard SelectedServer
        {
            get => _selectedServer;
            set
            {
                _selectedServer = value;
                if (value != null)
                    LoadPackageScriptAsync(value);
                OnPropertyChanged();
            }
        }

        public string SelectedPackageScript
        {
            get => _selectedServerInstallScriptText;
            set
            {
                _selectedServerInstallScriptText = value;
                OnPropertyChanged();
            }
        }

        public string Status
        {
            get => _statusMessage;
            set
            {
                _statusMessage = value;
                OnPropertyChanged();
            }
        }

        public bool Ready
        {
            get => _ready;
            set
            {
                _ready = value;
                OnPropertyChanged();
            }
        }

        public bool NetStatUpdateAvailable
        {
            get => _netUpdateAvailable;
            set
            {
                _netUpdateAvailable = value;
                OnPropertyChanged();
            }
        }

        public ImageSource RefreshIcon { get; set; }
        public ImageSource ReloadIcon { get; set; }

        public ObservableCollection<ServerSwapCard> Servers { get; }

        public RelayCommand InstallSelectedServer { get; set; }
        public RelayCommand RemoveSelectedServer { get; set; }
        public RelayCommand SaveInstallerScript { get; set; }
        public RelayCommand DiscardScriptChanges { get; set; }
        public RelayCommand DefineNewVariable { get; set; }
        public RelayCommand RemoveSelected { get; set; }
        public RelayCommand UpdateServersNetworkState { get; set; }
        public RelayCommand UpdateServersList { get; set; }
        public RelayCommand RemoveServerPackage { get; set; }
        public RelayCommand OpenGuidePage { get; set; }
        public InterpreterVariable VariableDefinitionProxy { get; set; }
        public InterpreterVariable SelectedVariableItem { get; set; }
        public ObservableCollection<InterpreterVariable> InterpreterVariables { get; set; }

        private string _selectedServerInstallScriptText;
        private ServerSwapCard _selectedServer;
        private string _statusMessage;

        private bool _ready;
        private bool _netUpdateAvailable = true;

        public void OnFileDrop(object sender, EventArgs rawEvent)
        {
            if (!(rawEvent is System.Windows.DragEventArgs e))
                return;

            if (!e.Data.GetDataPresent(DataFormats.FileDrop))
                return;

            var files = ((string[])e.Data.GetData(DataFormats.FileDrop))?.Where(file =>
                Path.GetExtension(file) == ".zip").ToList();
            if (files == null || files.Count == 0)
                return;


            foreach (var file in files)
            {
                var fileName = Path.GetFileNameWithoutExtension(file);

                var requiredFiles = new[]
                {
                    $"{fileName}-Package.s.tws",
                    $"{fileName}-Package.zip",
                    "remotes.specs"
                };

                try
                {
                    using (var zip = ZipFile.OpenRead(file))
                    {
                        var uniqueFilesBuffer = zip.Entries.ToHashSet();
                        if (zip.Entries.Count < 3 ||
                            !uniqueFilesBuffer.All(entry => entry.Name.SameWith(requiredFiles)))
                        {
                            ApplicationNotificationManager.Manager.ShowErrorWnd("Import Error", "Package is not valid. Not all files match requires or some files missing");
                            continue;
                        }

                        zip.ExtractToDirectory($"Servers\\{fileName}");
                        AddServerPackage($"Servers\\{fileName}");
                    }
                }
                catch (Exception ex) // if archive does not opens or anythings else happening wrong we just ignore it
                {
                    ApplicationNotificationManager.Manager.ShowErrorWnd("Error", $"Error while add package: {ex.Message}");
                }
            }
        }

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
            if (!_ready) return;

            Ready = false;
            var path = string.Copy(SelectedServer.BasePackagePath);
            var packageName = string.Copy(SelectedServer.ServerName);
            var selectedCard = SelectedServer;

            await Task.Run(() =>
            {
                var swapper = new ServerSwapper(InterpreterVariables.ToDictionary(
                                                key => key.Name,
                                                value => value.Value));

                swapper.SwapFailed += (s, e) =>
                {
                    ApplicationNotificationManager.Manager.ShowError("Fail!!", $"Swap Failure: {e.Message}");
                };

                swapper.SwapSuccessfull += (s, e) =>
                {
                    ApplicationNotificationManager.Manager.ShowSuccess("Done!", "Server pacakge successfully installed");
                    ConfigurationManager.UpdateSection("InstalledServerPackageName", packageName);
                    SettingsProvider.InstalledServerPackageName = packageName;
                    selectedCard.Installed = true;
                };

                Status = "Installing Package :: Working...";
                swapper.SwapServer(path);
            })
            .ContinueWith(task =>
            {
                UpdateStatusWithTaskResultAndNotify(task);
                Ready = true;
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
                .ContinueWith(task =>
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
                .ContinueWith(task =>
            {
                UpdateStatusWithTaskResultAndNotify(task);
            });
        }

        private async void RemoveSelectedServerInternal(object obj)
        {
            if (!_ready)
                return;
            Ready = false;
            var path = string.Copy(SelectedServer.BasePackagePath);
            var serverName = string.Copy(SelectedServer.ServerName);
            var selectedCard = SelectedServer;

            await Task.Run(() =>
            {
                var swapper = new ServerSwapper(InterpreterVariables.ToDictionary(
                                                key => key.Name,
                                                value => value.Value));
                swapper.SwapFailed += (s, e) =>
                {
                    ApplicationNotificationManager.Manager.ShowError("Fail!!", $"Swap Failure: {e.Message}");
                };

                swapper.SwapSuccessfull += (s, e) =>
                {
                    ApplicationNotificationManager.Manager.ShowSuccess("Done!", "Server pacakge successfully Removed");
                    ConfigurationManager.UpdateSection("InstalledServerPackageName", SettingsProvider.DefaultDylanServerName);
                    SettingsProvider.InstalledServerPackageName = SettingsProvider.DefaultDylanServerName;
                    selectedCard.Installed = false;
                };

                swapper.RemoveServerByPackage(path);
            })
            .ContinueWith(task =>
            {
                UpdateStatusWithTaskResultAndNotify(task);
                Ready = true;
            });
        }

        private void DefineNewNameInternal(object obj)
        {
            if (VariableDefinitionProxy.Name == "%PACKAGE_ROOT%")
            {
                ApplicationNotificationManager.Manager.ShowWarningWnd("Restricted Action", "Unable to add name %PACKAGE_ROOT% - reserved variable name, defined by interpreter itself");
                return;
            }
            var nameDefinitionProxyClone = VariableDefinitionProxy.Clone();
            InterpreterVariables.Add(nameDefinitionProxyClone);
        }

        private void UpdateStatusWithTaskResultAndNotify(Task task, bool showSuccess = false)
        {
            if (task.IsFaulted || task.IsCanceled)
            {
                Status = "Operation Failed :: Ready";
                ApplicationNotificationManager.Manager.ShowError("Fail!", $"Installation Error - {task.Exception?.Message}");
                return;
            }
            Status = "Operation Completed :: Ready";

            if (showSuccess) 
                ApplicationNotificationManager.Manager.ShowSuccess("Done!", "Operation Completed");
        }

        private void RemoveInterpreterDefine(object o)
        {
            if (SelectedVariableItem == null)
                return;

            if (SelectedVariableItem.Name.SameWith("%AS%", "%BACKUP_PATH%", "%PACKAGE_ROOT"))
            {
                ApplicationNotificationManager.Manager.ShowWarningWnd("Restricted Action", "Requiered Interpreter variable, can not remove");
                return;
            }

            InterpreterVariables.Remove(SelectedVariableItem);
        }

        private bool AssertPackageValid(string path)
        {
            if (!Directory.Exists(path))
                return false;

            var packageRootName = Path.GetDirectoryName(path);
            
            if (packageRootName == null)
                return false;
            
            var packageFiles = new HashSet<string>(Directory.EnumerateFiles(path));

            var requiredFiles = new[]
            {
                $"{packageRootName}-Package.s.tws",
                $"{packageRootName}-Package.zip",
                "remotes.specs"
            };

            return packageFiles.All(file => Path.GetFileName(file).SameWith(requiredFiles));
        }

        private void AddServerPackage(string package)
        {
            var card = new ServerSwapCard(Path.GetFullPath(package))
            {
                ServerName = package.Split('\\').Last()
            };
            var specs = Directory.EnumerateFiles(package).ToList().Find(f => f.EndsWith(".specs"));
            if (specs != null)
            {
                var specsList = File.ReadAllLines(specs).Select(x => x.Split('=')).ToDictionary(x => x[0], x => x[1]);
                if (specsList.ContainsKey("remote"))
                {
                    card.ServerHost = specsList["remote"];
                    card.SpecsServerRemote = specsList["remote"];
                }
            }

            Servers.Add(card);
        }

        private async void RemoveServerPackageInternal(object _)
        {
            var server = SelectedServer;
            var userApproval = ApplicationNotificationManager.Manager.AskForAction("Server remove",
                    "Are you sure to delete this package?");
            if (userApproval)
            {
                Servers.Remove(server);
                Utils.HardClear(server.BasePackagePath);
                try
                {
                    Directory.Delete(server.BasePackagePath);
                }
                catch
                {
                    // ignore
                }
            }
            else
            {
                ApplicationNotificationManager.Manager.ShowInformationWnd("", "Action was declined");
            }
        }

        private void LoadServers()
        {
            Servers.Clear();

            var packages = Directory.EnumerateDirectories(SettingsProvider.BaseServerPackagePath);

            foreach (var package in packages)
            {
                AddServerPackage(package);
            }
        }

        private void LoadServersCommand(object _)
        {
            LoadServers();
            ApplicationNotificationManager.Manager.ShowInformationWnd("", "Operation completed");
        }

        private async void UpdateServersNetStatsInternal(object _)
        {
            var tasks = new List<Task>(Servers.Count);

            NetStatUpdateAvailable = false;
            foreach (var server in Servers)
            {
                tasks.Add(server.ActualizeRemoteStats());
            }
            await Task.WhenAll(tasks.ToArray());
            ApplicationNotificationManager.Manager.ShowInformationWnd("Done!", "Servers network statistics updated");
            NetStatUpdateAvailable = true;
        }
    }
}
