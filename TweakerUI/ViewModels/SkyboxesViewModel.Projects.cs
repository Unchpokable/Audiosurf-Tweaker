using System;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerCore.Skyboxes;
using TweakerUI.Core;
using TweakerUI.Models;
using TweakerUI.Services;

namespace TweakerUI.ViewModels
{
    public partial class SkyboxesViewModel
    {
        private SkyProjectRowViewModel _awaitingInstall;
        private SkySelection _awaitingSky;

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(NameProblem), nameof(PackageHint), nameof(RenameWarning))]
        private string projectName = "";

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(AuthorProblem))]
        private string projectAuthor = "";

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(VersionProblem))]
        private string projectVersion = "1";

        [ObservableProperty]
        private SkyTemplate template = SkyTemplates.All[0];

        [ObservableProperty]
        private string formProblem;

        public string NameProblem => SkyPackageName.Problem(ProjectName);
        public string AuthorProblem => SkyManifest.AuthorProblem(ProjectAuthor);
        public string VersionProblem => SkyManifest.VersionProblem(ProjectVersion);
        public string PackageHint => NameProblem == null ? "Installs as " + SkyPackageName.FileName(ProjectName) : "The name is also the package file name.";
        public string RenameWarning => IsEditing && SelectedProject?.PackageFileName != null && NameProblem == null
            && !string.Equals(SkyPackageName.FileName(ProjectName), SelectedProject.PackageFileName, StringComparison.OrdinalIgnoreCase)
            ? "The next install writes a new package. " + SelectedProject.PackageFileName + " already in the game stays until you delete it on the Game page." : null;

        private void ReloadProjects()
        {
            var keep = SelectedProject?.Root;
            var old = Projects.ToArray();
            Projects.Clear();
            foreach (var project in _projects.List())
            {
                var row = old.FirstOrDefault(item => string.Equals(item.Root, project.Root, StringComparison.OrdinalIgnoreCase));
                if (row == null)
                    row = new SkyProjectRowViewModel(project);
                else
                    row.Update(project);

                Projects.Add(row);
            }

            SelectedProject = Projects.FirstOrDefault(row => string.Equals(row.Root, keep, StringComparison.OrdinalIgnoreCase));
        }

        [RelayCommand]
        private void RefreshProjects() => ReloadProjects();

        partial void OnSelectedProjectChanged(SkyProjectRowViewModel value)
        {
            if (value != null)
                IsCreating = false;

            IsEditing = false;
            FormProblem = null;
            WatchProject();
            if (value != null && !value.IsMissing)
                _ = PrepareEditorAsync(value);
        }

        private async Task PrepareEditorAsync(SkyProjectRowViewModel row)
        {
            try
            {
                await Task.Run(() => SkyEditorSupport.Refresh(row.Root));
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                Logger.Log("Skyboxes", "cannot prepare shader headers: " + exception.Message);
            }
        }

        partial void OnInstallOnSaveChanged(bool value) => WatchProject();

        private void WatchProject()
        {
            _projectWatcher.Stop();
            if (InstallOnSave && SelectedProject != null && !SelectedProject.IsMissing)
                _projectWatcher.Start(SelectedProject.Root);
        }

        private Task AutoInstallAsync() => InstallOnSave && SelectedProject != null
            ? InstallProjectAsync(SelectedProject, use: false) : Task.CompletedTask;

        [RelayCommand]
        private void StartCreate()
        {
            ProjectName = "";
            ProjectAuthor = "";
            ProjectVersion = "1";
            Template = Templates[0];
            FormProblem = null;
            IsEditing = false;
            IsCreating = true;
        }

        [RelayCommand]
        private void CancelForm()
        {
            IsCreating = false;
            IsEditing = false;
            FormProblem = null;
        }

        [RelayCommand]
        private async Task Create()
        {
            if (Busy || NameProblem != null || AuthorProblem != null)
                return;

            var name = ProjectName;
            var path = _projects.NewProjectPath(name);
            Busy = true;
            try
            {
                var key = (Template ?? Templates[0]).Key;
                var author = ProjectAuthor;
                await Task.Run(() => SkyTemplates.Create(key, path, name, author));
                _projects.Remember(path);
                IsCreating = false;
                ReloadProjects();
                SelectedProject = Projects.FirstOrDefault(row => string.Equals(row.Root, path, StringComparison.OrdinalIgnoreCase));
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException || exception is JsonException)
            {
                FormProblem = "Could not create the project: " + exception.Message;
            }
            finally
            {
                Busy = false;
            }
        }

        [RelayCommand]
        private async Task OpenExisting()
        {
            var folder = await FileDialogService.OpenFolderAsync("Open a sky project (a folder with Config.json)");
            if (folder != null)
                OpenProject(folder);
        }

        /// <summary>Opens a project in place; also used by UiStand without a platform folder picker.</summary>
        public bool OpenProject(string folder)
        {
            if (!_projects.Remember(folder))
            {
                ApplicationNotificationManager.Manager.ShowWarning("Skyboxes", "This folder has no Config.json. Create a project or choose a sky package folder.");
                return false;
            }

            ReloadProjects();
            SelectedProject = Projects.FirstOrDefault(row => string.Equals(Path.GetFullPath(row.Root), Path.GetFullPath(folder), StringComparison.OrdinalIgnoreCase));
            IsCreating = false;
            return true;
        }

        [RelayCommand]
        private void StartEdit()
        {
            var project = SelectedProject?.Project;
            if (project == null || project.IsMissing || Busy)
                return;

            ProjectName = project.Name ?? "";
            ProjectAuthor = project.Author ?? "";
            ProjectVersion = (project.Version ?? 1).ToString(CultureInfo.InvariantCulture);
            FormProblem = null;
            IsEditing = true;
            OnPropertyChanged(nameof(RenameWarning));
        }

        [RelayCommand]
        private void SaveEdit()
        {
            var row = SelectedProject;
            if (row == null || Busy || NameProblem != null || AuthorProblem != null || VersionProblem != null)
                return;

            try
            {
                SkyManifest.Update(row.Root, ProjectName, ProjectAuthor, int.Parse(ProjectVersion, CultureInfo.InvariantCulture));
                IsEditing = false;
                ReloadProjects();
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException || exception is JsonException)
            {
                FormProblem = "Could not save Config.json: " + exception.Message;
            }
        }

        [RelayCommand]
        private void OpenProjectFolder()
        {
            if (SelectedProject != null && !SelectedProject.IsMissing)
                ShellActions.OpenFolder("Skyboxes", SelectedProject.Root);
        }

        [RelayCommand]
        private void ForgetProject()
        {
            if (SelectedProject?.CanForget != true || Busy)
                return;

            _projects.Forget(SelectedProject.Root);
            SelectedProject = null;
            ReloadProjects();
        }

        [RelayCommand]
        private async Task DeleteProject()
        {
            var row = SelectedProject;
            if (row == null || row.IsMissing || Busy)
                return;

            if (!await ApplicationNotificationManager.Manager.AskForAction("Delete sky project",
                $"Send {row.Root} and everything in it to the Recycle Bin? The installed package stays in the game; remove it on the Game page separately."))
                return;

            _projectWatcher.Stop();
            var error = await ShellActions.RecycleFolderAsync(row.Root);
            if (error != null)
            {
                ApplicationNotificationManager.Manager.ShowError("Skyboxes", error);
                WatchProject();
                return;
            }

            _projects.Forget(row.Root);
            SelectedProject = null;
            ReloadProjects();
        }

        [RelayCommand]
        private Task Install() => InstallProjectAsync(SelectedProject, use: false);

        [RelayCommand]
        private Task InstallAndUse() => InstallProjectAsync(SelectedProject, use: true);

        private async Task InstallProjectAsync(SkyProjectRowViewModel row, bool use)
        {
            if (row == null || row.IsMissing || Busy)
                return;

            var engine = PluginService.EngineDirectory;
            if (engine == null)
            {
                row.Result = "Not installed: set the game textures path in Settings.";
                return;
            }

            if (use && !SkyboxOverlayBridge.IsLive && (GameRunning || GameProcessService.IsRunning))
            {
                row.Result = "Not installed: connect to the running game to install and select this sky. Install without selecting is still available.";
                return;
            }

            Busy = true;
            _projectWatcher.WorkStarted();
            row.Result = "Checking and packing…";
            try
            {
                var result = await Task.Run(() =>
                {
                    SkyEditorSupport.Refresh(row.Root);
                    return SkyPackager.Install(row.Root, engine, CompressionLevel.Fastest);
                });
                ShowProblems(row, result);
                if (!result.Succeeded)
                    return;

                var sky = new SkySelection(SkyKind.Package, result.FileName);
                var live = SkyboxOverlayBridge.IsLive;
                var active = live && SkyboxOverlayBridge.State?.Sky.Matches(sky.Kind, sky.Id) == true;
                SkyboxOverlayBridge.Rescan();
                if (use || active)
                {
                    _awaitingInstall = live ? row : null;
                    _awaitingSky = sky;
                    var selected = ChangeSelection(null, sky);
                    row.Result = selected
                        ? live ? "Installed; waiting for the game to load it…" : "Installed and selected at " + DateTime.Now.ToShortTimeString()
                        : "Installed; could not select it. Connect to the game or select it in the overlay.";
                    if (!selected)
                        _awaitingInstall = null;
                    else
                        UpdateInstallState();
                }
                else
                {
                    row.Result = "Installed at " + DateTime.Now.ToShortTimeString();
                }

                row.Update(SkyProjects.Read(row.Root, row.Project.InSources));
                await RefreshAsync();
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException || exception is JsonException)
            {
                row.Result = "Not installed: " + exception.Message;
            }
            finally
            {
                Busy = false;
                _projectWatcher.WorkFinished();
            }
        }

        private void UpdateInstallState()
        {
            var row = _awaitingInstall;
            if (row == null)
                return;

            if (!SkyboxOverlayBridge.IsLive)
            {
                row.Result = "Installed; connection lost before the game confirmed loading it.";
                _awaitingInstall = null;
                return;
            }

            var state = SkyboxOverlayBridge.State;
            if (SkyboxOverlayBridge.IsPending || state == null)
                return;

            if (!state.Sky.Matches(_awaitingSky.Kind, _awaitingSky.Id))
            {
                row.Result = "Installed; the game selected a different sky.";
            }
            else if (state.Status == SkyLiveStatus.Compiling)
            {
                row.Result = "Installed; the game is compiling it…";
                return;
            }
            else if (state.Status is SkyLiveStatus.Failed or SkyLiveStatus.Missing)
            {
                row.Result = "Installed; the game could not load it: " + (state.Message.Length > 0 ? state.Message : state.Status.ToString().ToLowerInvariant());
            }
            else
            {
                row.Result = "Installed and selected at " + DateTime.Now.ToShortTimeString()
                    + (state.Status == SkyLiveStatus.Waiting ? " · waiting for the game to draw the sky" : state.Status == SkyLiveStatus.Off ? " · replacer is off" : " · live from the game");
            }

            _awaitingInstall = null;
        }

        private static void ShowProblems(SkyProjectRowViewModel row, SkyInstallResult result, string failure = "Not installed")
        {
            row.Problems.Clear();
            foreach (var problem in result.Problems)
                row.Problems.Add(problem);

            if (!result.Succeeded)
                row.Result = result.Message ?? $"{failure}: {result.Problems.Count(problem => problem.IsError)} problems.";
        }

        [RelayCommand]
        private async Task Export()
        {
            var row = SelectedProject;
            if (row == null || row.IsMissing || Busy)
                return;

            var path = await FileDialogService.SaveFileAsync("Pack sky to file", row.PackageFileName ?? "sky.sky");
            if (path != null)
                await ExportProjectAsync(row, path);
        }

        public async Task ExportProjectAsync(SkyProjectRowViewModel row, string path)
        {
            if (row == null || Busy)
                return;

            Busy = true;
            try
            {
                var result = await Task.Run(() => SkyPackager.Export(row.Root, path));
                ShowProblems(row, result, "Not packed");
                if (result.Succeeded)
                    row.Result = "Packed to " + result.Path;
            }
            finally
            {
                Busy = false;
            }
        }

        [RelayCommand]
        private Task OpenInEditor() => SelectedProject == null || SelectedProject.IsMissing ? Task.CompletedTask
            : Editor.OpenAsync(SelectedProject.Root, null, 0, 0);

        [RelayCommand]
        private Task OpenProblem(SkyProblem problem) => SelectedProject == null ? Task.CompletedTask
            : Editor.OpenAsync(SelectedProject.Root, SkyProjects.ManifestFileName, 0, 0);
    }
}
