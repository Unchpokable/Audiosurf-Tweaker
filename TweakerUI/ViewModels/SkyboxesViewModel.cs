using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerCore.DevProjects;
using TweakerCore.Skyboxes;
using TweakerUI.Core;
using TweakerUI.Models;

namespace TweakerUI.ViewModels
{
    /// <summary>Game sky selection and author projects. The running plugin owns module.json; offline we edit it.</summary>
    public partial class SkyboxesViewModel : ViewModelBase, IDisposable
    {
        private readonly SkyProjects _projects = new SkyProjects(DevTools.SkySources, DevTools.RecentSkyProjectsFile);
        private readonly ProjectWatcher _projectWatcher;
        private readonly DispatcherTimer _gamePoll;
        private readonly DispatcherTimer _rescan;
        private FileSystemWatcher _gameWatcher;
        private string _watchedEngine;
        private IReadOnlyList<GameSky> _diskSkies = Array.Empty<GameSky>();
        private SkyModuleConfig _config = new SkyModuleConfig();
        private bool _active;
        private bool _applying;
        private bool _disposed;
        private int _scanGeneration;

        public SkyboxesViewModel()
        {
            _projectWatcher = new ProjectWatcher("Skyboxes",
                new[] { ".git", ".vscode", ".vs", ".idea", ".tweaker", ".agents", ".claude", SkyPackager.ShaderToolsConfigFileName },
                TimeSpan.FromMilliseconds(300), AutoInstallAsync);
            _gamePoll = new DispatcherTimer { Interval = TimeSpan.FromSeconds(2) };
            _gamePoll.Tick += (_, _) => UpdateGameRunning();
            _rescan = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(300) };
            _rescan.Tick += (_, _) =>
            {
                _rescan.Stop();
                _ = RefreshAsync();
            };

            Editor = new EditorPickerViewModel("Skyboxes", () => EditorEnvironment.None, null, null);
            SkyboxOverlayBridge.Changed += OnBridgeChanged;
            SkyboxOverlayBridge.RequestTimedOut += OnRequestTimedOut;
        }

        public EditorPickerViewModel Editor { get; }
        public ObservableCollection<SkyRowViewModel> Skies { get; } = new ObservableCollection<SkyRowViewModel>();
        public ObservableCollection<SkyProjectRowViewModel> Projects { get; } = new ObservableCollection<SkyProjectRowViewModel>();
        public IReadOnlyList<SkyTemplate> Templates => SkyTemplates.All;

        [ObservableProperty]
        private int page;

        [ObservableProperty]
        private string notice;

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(ProjectGameStatus))]
        private string activeText;

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(ProjectGameStatus))]
        private string loadProblem;

        [ObservableProperty]
        private string emptyText;

        [ObservableProperty]
        private bool canControl;

        [ObservableProperty]
        private bool gameRunning;

        [ObservableProperty]
        private bool enabled;

        [ObservableProperty]
        private SkyRowViewModel selectedSky;

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(ShowDetails), nameof(ShowNoProject), nameof(ProjectGameStatus))]
        private SkyProjectRowViewModel selectedProject;

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(ShowDetails), nameof(ShowNoProject), nameof(ShowForm))]
        private bool isCreating;

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(ShowDetails), nameof(ShowNoProject), nameof(ShowForm))]
        private bool isEditing;

        [ObservableProperty]
        private bool busy;

        [ObservableProperty]
        private bool installOnSave;

        public bool ShowForm => IsCreating || IsEditing;
        public bool ShowDetails => SelectedProject != null && !ShowForm;
        public bool ShowNoProject => SelectedProject == null && !ShowForm;

        public string ProjectGameStatus => SkyboxOverlayBridge.IsLive && SelectedProject?.PackageFileName != null
            && SkyboxOverlayBridge.State?.Sky.Matches(SkyKind.Package, SelectedProject.PackageFileName) == true
            ? "In the game: " + SkyboxOverlayBridge.State.Status.ToString().ToLowerInvariant()
                + (SkyboxOverlayBridge.State.Message.Length > 0 ? " · " + SkyboxOverlayBridge.State.Message : "") : null;

        partial void OnPageChanged(int value)
        {
            if (_active && value == 0)
            {
                UpdateGameRunning();
                _ = RefreshAsync();
            }
        }

        public void SetActive(bool active)
        {
            if (_active == active || _disposed)
                return;

            _active = active;
            if (active)
            {
                UpdateGameRunning();
                _gamePoll.Start();
                ReloadProjects();
                _ = RefreshAsync();
                _ = Editor.DetectAsync();
            }
            else
            {
                _scanGeneration++;
                _gamePoll.Stop();
                _rescan.Stop();
                StopWatchingGame();
            }
        }

        public void Dispose()
        {
            SetActive(false);
            _disposed = true;
            SkyboxOverlayBridge.Changed -= OnBridgeChanged;
            SkyboxOverlayBridge.RequestTimedOut -= OnRequestTimedOut;
            _projectWatcher.Dispose();
            Editor.Dispose();
        }

        [RelayCommand]
        private async Task Refresh()
        {
            UpdateGameRunning();
            SkyboxOverlayBridge.Rescan();
            await RefreshAsync();
        }

        public async Task RefreshAsync()
        {
            var generation = ++_scanGeneration;
            var engine = PluginService.EngineDirectory;
            var payload = PluginService.PayloadDirectory;
            var snapshot = await Task.Run(() => (
                Skies: engine == null ? Array.Empty<GameSky>() : GameSkies.Scan(engine, payload),
                Config: engine == null ? new SkyModuleConfig() : SkyModuleConfig.Load(engine)));
            if (_disposed || generation != _scanGeneration)
                return;

            _diskSkies = snapshot.Skies;
            _config = snapshot.Config;
            WatchGame(engine);
            ApplyState();
        }

        private void OnBridgeChanged(object sender, EventArgs e)
        {
            UpdateInstallState();
            if (_active)
                ApplyState();
        }

        private void OnRequestTimedOut(object sender, EventArgs e)
        {
            if (_awaitingInstall != null)
            {
                _awaitingInstall.Result = "Installed; the game did not confirm loading it. Try selecting it again in the Game page or the overlay.";
                _awaitingInstall = null;
            }

            ApplicationNotificationManager.Manager.ShowWarning("Skyboxes",
                "The game did not confirm the sky change. The controls show its last state. Try again, or use the Skybox tab in the game's overlay.");
        }

        private void UpdateGameRunning()
        {
            var running = GameProcessService.IsRunning || SkyboxOverlayBridge.IsLive;
            if (running == GameRunning)
                return;

            GameRunning = running;
            _ = RefreshAsync();
        }

        private SkySelection CurrentSelection => SkyboxOverlayBridge.IsLive
            ? SkyboxOverlayBridge.PendingSelection ?? SkyboxOverlayBridge.State?.Sky : _config.Sky;

        private void ApplyState()
        {
            var live = SkyboxOverlayBridge.IsLive;
            var engine = PluginService.EngineDirectory;
            var selection = CurrentSelection;
            CanControl = engine != null && (live ? SkyboxOverlayBridge.State != null : !GameRunning && _config.Problem == null);
            Notice = engine == null ? "Audiosurf's folder is unknown. Set the game textures path in Settings."
                : GameRunning && !live ? "The game is running without a connection. Select skies in the Skybox tab of the game's overlay, or connect Tweaker in Settings."
                : !live && _config.Problem != null ? _config.Problem + ". Open the sky folder to fix module.json; Tweaker leaves it as it is."
                : !PluginService.IsInstalled ? "The plugin is not installed. Install it in Settings so the game can replace the sky."
                : live && !SkyboxOverlayBridge.IsSynced ? "Waiting for the game's sky catalog…" : null;

            _applying = true;
            try
            {
                Enabled = live ? SkyboxOverlayBridge.PendingEnabled ?? SkyboxOverlayBridge.State?.Enabled ?? _config.Enabled : _config.Enabled;
                var entries = live ? SkyboxOverlayBridge.Catalog
                    : _diskSkies.Select(sky => new SkyCatalogEntry(sky.Kind, sky.Id, sky.DisplayName, sky.FaceSize)).ToArray();

                // Keep row objects across status updates: replacing the list resets keyboard focus and scroll position.
                var rows = entries.Select(entry =>
                {
                    var disk = _diskSkies.FirstOrDefault(item => new SkySelection(entry.Kind, entry.Id).Matches(item.Kind, item.Id));
                    var path = disk?.FullPath;
                    var project = Projects.FirstOrDefault(item => entry.Kind == SkyKind.Package
                        && string.Equals(item.PackageFileName, entry.Id, StringComparison.OrdinalIgnoreCase));
                    var row = Skies.FirstOrDefault(item => item.Entry == entry && item.FullPath == path
                        && item.IsShipped == (disk?.IsShipped == true) && item.ProjectName == project?.Title);
                    if (row != null)
                        return row;

                    return new SkyRowViewModel(entry, path, disk?.IsShipped == true, project?.Title);
                }).ToArray();

                for (var i = 0; i < rows.Length; i++)
                {
                    if (i < Skies.Count && ReferenceEquals(Skies[i], rows[i]))
                        continue;

                    var previous = Skies.IndexOf(rows[i]);
                    if (previous >= 0)
                        Skies.Move(previous, i);
                    else
                        Skies.Insert(i, rows[i]);
                }
                while (Skies.Count > rows.Length)
                    Skies.RemoveAt(Skies.Count - 1);

                foreach (var row in Skies)
                    row.IsActive = selection?.Matches(row.Entry.Kind, row.Entry.Id) == true;

                SelectedSky = Skies.FirstOrDefault(row => row.IsActive);
                var name = SelectedSky?.Title ?? (selection == null ? "Waiting for the game…"
                    : (SkyKinds.IsOnDisk(selection.Kind) ? "" : "built-in ") + selection.Id);
                var state = SkyboxOverlayBridge.State;
                ActiveText = "Active: " + name + (live && state != null ? " · " + state.Status.ToString().ToLowerInvariant() + " · live from the game" : "")
                    + (SkyboxOverlayBridge.IsPending ? " · Waiting for the game…" : "");
                LoadProblem = live && state?.Status == SkyLiveStatus.Failed
                    ? state.Message + " — Full diagnostics: Skybox tab in the game's overlay." : null;
                EmptyText = Skies.Count != 0 ? null : live ? "The game has not sent any skies yet."
                    : engine == null ? "Set the game folder in Settings to see installed skies."
                    : "No skies in the game yet. Create one on the Projects page, or install the bundled skies in Settings.";
            }
            finally
            {
                _applying = false;
            }

            OnPropertyChanged(nameof(ProjectGameStatus));
        }

        partial void OnEnabledChanged(bool value)
        {
            if (_applying)
                return;

            ChangeSelection(value, null);
        }

        partial void OnSelectedSkyChanged(SkyRowViewModel value)
        {
            if (!_applying && value != null)
            {
                if (CanControl)
                    ChangeSelection(null, new SkySelection(value.Entry.Kind, value.Entry.Id));
                else
                    ApplyState();
            }
        }

        /// <summary>Rechecks the process immediately before an offline write, including after packaging finishes.</summary>
        private bool ChangeSelection(bool? replace, SkySelection sky)
        {
            if (SkyboxOverlayBridge.IsLive)
            {
                var sent = replace != null ? SkyboxOverlayBridge.RequestEnable(replace.Value) : SkyboxOverlayBridge.RequestSelect(sky);
                ApplyState();
                return sent;
            }

            if (GameRunning || GameProcessService.IsRunning || PluginService.EngineDirectory == null)
            {
                ApplyState();
                return false;
            }

            try
            {
                SkyModuleConfig.Write(PluginService.EngineDirectory, replace, sky);
                _config = SkyModuleConfig.Load(PluginService.EngineDirectory);
                ApplyState();
                return true;
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                ApplicationNotificationManager.Manager.ShowError("Skyboxes", "Could not change the sky: " + exception.Message);
                ApplyState();
                return false;
            }
        }

        [RelayCommand]
        private void OpenGameFolder()
        {
            var engine = PluginService.EngineDirectory;
            if (engine != null)
                ShellActions.OpenFolder("Skyboxes", SkyPaths.ReplacerPath(engine));
        }

        [RelayCommand]
        private void RevealSky(SkyRowViewModel row)
        {
            if (row?.FullPath != null)
                ShellActions.OpenFolder("Skyboxes", Directory.Exists(row.FullPath) ? row.FullPath : Path.GetDirectoryName(row.FullPath));
        }

        [RelayCommand]
        private async Task DeleteSky(SkyRowViewModel row)
        {
            var engine = PluginService.EngineDirectory;
            if (row?.CanDelete != true || engine == null || !IsSkyPath(engine, row.FullPath)
                || GameSkies.IsShipped(engine, PluginService.PayloadDirectory, row.Entry.Id))
                return;

            var warning = row.IsActive ? " The game may keep drawing it in this session; it will be unavailable on the next start." : "";
            if (!await ApplicationNotificationManager.Manager.AskForAction("Delete sky",
                $"Send {row.Entry.Id} to the Recycle Bin?" + warning))
                return;

            var error = Directory.Exists(row.FullPath) ? await ShellActions.RecycleFolderAsync(row.FullPath)
                : await ShellActions.RecycleFileAsync(row.FullPath);
            if (error != null)
                ApplicationNotificationManager.Manager.ShowError("Skyboxes", "Could not delete the sky: " + error);

            SkyboxOverlayBridge.Rescan();
            await RefreshAsync();
        }

        private static bool IsSkyPath(string engine, string path) => path != null
            && string.Equals(Path.GetDirectoryName(Path.GetFullPath(path)), Path.GetFullPath(SkyPaths.SkyboxesPath(engine)), StringComparison.OrdinalIgnoreCase);

        private void WatchGame(string engine)
        {
            if (!_active || string.Equals(engine, _watchedEngine, StringComparison.OrdinalIgnoreCase))
                return;

            StopWatchingGame();
            if (engine == null || !Directory.Exists(SkyPaths.ReplacerPath(engine)))
                return;

            try
            {
                _gameWatcher = new FileSystemWatcher(SkyPaths.ReplacerPath(engine))
                {
                    IncludeSubdirectories = true,
                    NotifyFilter = NotifyFilters.FileName | NotifyFilters.DirectoryName | NotifyFilters.LastWrite | NotifyFilters.Size
                };
                void Changed(object sender, FileSystemEventArgs e)
                {
                    var relative = Path.GetRelativePath(SkyPaths.ReplacerPath(engine), e.FullPath);
                    if (!string.Equals(relative, SkyPaths.ModuleConfigFileName, StringComparison.OrdinalIgnoreCase)
                        && !string.Equals(relative, SkyPaths.SkyboxesFolderName, StringComparison.OrdinalIgnoreCase)
                        && !relative.StartsWith(SkyPaths.SkyboxesFolderName + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                        return;

                    Dispatcher.UIThread.Post(() =>
                    {
                        if (_active && !_disposed)
                        {
                            _rescan.Stop();
                            _rescan.Start();
                        }
                    });
                }
                _gameWatcher.Changed += Changed;
                _gameWatcher.Created += Changed;
                _gameWatcher.Deleted += Changed;
                _gameWatcher.Renamed += (sender, e) => Changed(sender, e);
                _gameWatcher.EnableRaisingEvents = true;
                _watchedEngine = engine;
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                Logger.Log("Skyboxes", "cannot watch the game's sky folder: " + exception.Message);
                StopWatchingGame();
            }
        }

        private void StopWatchingGame()
        {
            _gameWatcher?.Dispose();
            _gameWatcher = null;
            _watchedEngine = null;
        }
    }
}
