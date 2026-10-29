using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerCore.DevProjects;
using TweakerCore.Scripting;
using TweakerUI.Core;
using TweakerUI.Models;
using TweakerUI.Services;

namespace TweakerUI.ViewModels
{
    /// <summary>
    /// The Script Dev tab (Docs/Internal/script-manager.md §6): twllink projects - the ones in
    /// DevTools\Sources\ and the ones opened from anywhere else - created from a template, built, installed into
    /// the game, with the diagnostics of the last build. Everything twllink does is done by running twllink
    /// (TweakerCore.Scripting.Twllink); this tab only decides when and shows what came back.
    ///
    /// "Rebuild on save" is this tab's, not twllink's (§8.8): a watcher on the selected project's folder,
    /// running while the switch is on - including when another tab is on screen, because the user asked for it
    /// and is most likely looking at their editor, not at Tweaker.
    /// </summary>
    public partial class ScriptDevViewModel : ViewModelBase, IDisposable
    {
        /// <summary>Quiet time after the last change before a rebuild: one save is often several writes.</summary>
        private static readonly TimeSpan RebuildSettle = TimeSpan.FromMilliseconds(300);

        /// <summary>
        /// Folders of a project that are not its sources: twllink's output, editor state, and the AI agent setup -
        /// opening a project rewrites .tweaker\local.json, which must not count as a save.
        /// </summary>
        private static readonly string[] IgnoredFolders = { "build", ".twllink", ".git", ".vscode", ".vs", ".idea", ".tweaker", ".agents", ".claude" };

        private readonly ScriptProjects _projects;
        private readonly Twllink _twllink;
        private readonly HashSet<string> _collisionConfirmed = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private readonly ProjectWatcher _watcher;
        private bool _active;

        public ScriptDevViewModel()
        {
            _projects = new ScriptProjects(DevTools.Sources, DevTools.RecentProjectsFile);
            _twllink = DevTools.Twllink;
            _watcher = new ProjectWatcher("Script Dev", IgnoredFolders, RebuildSettle, AutoBuildAsync);

            Editor = new EditorPickerViewModel("Script Dev",
                () => EditorEnvironmentFor(PluginService.GameRootDirectory),
                "Opened with twllink and cgrpy on its PATH, so the editor's terminal can run them.",
                "twllink");
            Editor.PropertyChanged += (_, e) =>
            {
                if (e.PropertyName == nameof(EditorPickerViewModel.SelectedEditor))
                    OpenInEditorCommand.NotifyCanExecuteChanged();
            };
        }

        /// <summary>The Editor card (EditorPickerView), shared with the Skyboxes tab.</summary>
        public EditorPickerViewModel Editor { get; }

        /// <summary>
        /// What an editor opened from this tab gets (script-manager.md §8.3): twllink and cgrpy at the front of its
        /// PATH and AUDIOSURF_DIR at the game, so a terminal inside it runs "twllink --install" or "cgrpy query ..." as
        /// it is. Public for the UI stand, which starts Neovim with exactly this.
        /// </summary>
        public static EditorEnvironment EditorEnvironmentFor(string gameRoot)
        {
            return new EditorEnvironment
            {
                PathFront = DevTools.ToolFolders,
                Variables = new Dictionary<string, string> { ["AUDIOSURF_DIR"] = gameRoot }
            };
        }

        public ObservableCollection<ScriptProjectRowViewModel> Projects { get; } = new ObservableCollection<ScriptProjectRowViewModel>();

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(BuildCommand), nameof(InstallCommand), nameof(OpenProjectFolderCommand), nameof(ForgetCommand), nameof(OpenInEditorCommand), nameof(StartEditCommand), nameof(DeleteProjectCommand))]
        private ScriptProjectRowViewModel selected;

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(ShowDetails))]
        private bool hasSelection;

        [ObservableProperty]
        private bool hasProjects;

        [ObservableProperty]
        private string notice;

        [ObservableProperty]
        private bool hasNotice;

        /// <summary>The new-project form is open, in place of the selected project's details.</summary>
        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(ShowDetails))]
        private bool isCreating;

        /// <summary>The selected project's manifest fields are being edited.</summary>
        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(ShowDetails))]
        private bool isEditing;

        public bool ShowDetails => HasSelection && !IsCreating;

        public bool ShowNothingSelected => !HasSelection && !IsCreating;

        partial void OnHasSelectionChanged(bool value) => OnPropertyChanged(nameof(ShowNothingSelected));

        partial void OnIsCreatingChanged(bool value) => OnPropertyChanged(nameof(ShowNothingSelected));

        /// <summary>Where a new project goes, shown under the form.</summary>
        public string NewProjectFolderHint => string.IsNullOrEmpty(NewId)
            ? "Created in " + DevTools.Sources
            : "Created in " + Path.Combine(DevTools.Sources, NewId);

        /// <summary>What a new project can start from - twllink's --new templates.</summary>
        public IReadOnlyList<ProjectTemplate> Templates { get; } = new[]
        {
            new ProjectTemplate("hud", "HUD", "Draws over the game with tw.hud - the most common kind of script."),
            new ProjectTemplate("hook", "Hook", "Counts something the game does, through tw.on_call, and draws the count."),
            new ProjectTemplate("empty", "Empty", "Just the project, with an empty main.lua.")
        };

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(BuildCommand), nameof(InstallCommand))]
        private bool busy;

        /// <summary>Rebuild the selected project whenever one of its files changes.</summary>
        [ObservableProperty]
        private bool rebuildOnSave;

        /// <summary>...and install each rebuild into the game.</summary>
        [ObservableProperty]
        private bool installOnRebuild;

        public bool CanInstall => PluginService.EngineDirectory != null;

        public string InstallHint => CanInstall
            ? "Build, then copy the script and its .lua.map into the game's Scripts folder"
            : "Audiosurf's folder is unknown - set the game textures path in Settings";

        /// <summary>Called by MainWindowViewModel when the tab is shown or left.</summary>
        public void SetActive(bool active)
        {
            if (_active == active)
                return;
            _active = active;
            if (active)
            {
                Reload();
                UpdateNotice();
                OnPropertyChanged(nameof(CanInstall));
                OnPropertyChanged(nameof(InstallHint));
                _ = Editor.DetectAsync();
                _ = DetectUvAsync();
                _ = RefreshAgentsAsync();
            }
        }

        public void Dispose()
        {
            _watcher.Dispose();
            Editor.Dispose();
        }

        // --- the list -------------------------------------------------------------------------------------

        [RelayCommand]
        private void Refresh() => Reload();

        private void Reload()
        {
            var selectedRoot = Selected?.Root;
            var fresh = _projects.List();

            // Rows are kept by folder, so each one's last build survives a refresh.
            var byRoot = Projects.ToDictionary(p => Normalize(p.Root), StringComparer.OrdinalIgnoreCase);
            Projects.Clear();
            foreach (var project in fresh)
            {
                if (byRoot.TryGetValue(Normalize(project.Root), out var row))
                    row.Update(project);
                else
                    row = new ScriptProjectRowViewModel(project);
                Projects.Add(row);
            }

            HasProjects = Projects.Count > 0;
            Selected = Projects.FirstOrDefault(p => string.Equals(p.Root, selectedRoot, StringComparison.OrdinalIgnoreCase))
                       ?? Projects.FirstOrDefault(p => !p.IsMissing);
        }

        partial void OnSelectedChanged(ScriptProjectRowViewModel value)
        {
            HasSelection = value != null;
            // Picking a project in the list is leaving whatever form was open: its details are what was asked for.
            IsCreating = false;
            IsEditing = false;
            IsAgentPanelOpen = false;
            Watch();
            _ = RefreshAgentsAsync();
        }

        partial void OnRebuildOnSaveChanged(bool value) => Watch();

        partial void OnInstallOnRebuildChanged(bool value)
        {
            if (value && Selected != null)
                _ = ConfirmAutoInstallAsync(Selected);
        }

        /// <summary>
        /// Asked when the switch goes on, rather than on the first save - a dialog popping up while the user types
        /// in their editor is the wrong moment.
        /// </summary>
        private async Task ConfirmAutoInstallAsync(ScriptProjectRowViewModel row)
        {
            if (!await ConfirmCollisionIfNeededAsync(row))
                InstallOnRebuild = false;
        }

        private void UpdateNotice()
        {
            Notice = _twllink.Exists
                ? null
                : $"twllink.exe is missing from {DevTools.Binaries} - this copy of Tweaker was built or unpacked without it, so projects cannot be built.";
            HasNotice = Notice != null;
        }

        // --- creating and opening -------------------------------------------------------------------------

        // The form asks for the two names a project has, and says why they differ: the name is what people see (free
        // text, "PuzzlePro HUD"), the id is the script's file name in the game and its identity there, which is why it
        // is lowercase letters, digits and '_' (ProjectManifest has the reasons). The id follows the name as it is
        // typed, until the user edits the id itself.

        [ObservableProperty]
        private string newName = "";

        [ObservableProperty]
        private string newId = "";

        [ObservableProperty]
        private string newDescription = "";

        [ObservableProperty]
        private ProjectTemplate newTemplate;

        [ObservableProperty]
        private string newNameProblem;

        [ObservableProperty]
        private string newIdProblem;

        [ObservableProperty]
        private string createProblem;

        private bool _idTyped;
        private bool _settingIdFromName;

        partial void OnNewNameChanged(string value)
        {
            if (_idTyped)
                return;
            _settingIdFromName = true;
            NewId = ProjectManifest.IdFrom(value);
            _settingIdFromName = false;
        }

        partial void OnNewIdChanged(string value)
        {
            if (!_settingIdFromName)
                _idTyped = true;
            OnPropertyChanged(nameof(NewProjectFolderHint));
        }

        [RelayCommand]
        private void StartCreate()
        {
            NewName = "";
            NewDescription = "";
            _idTyped = false;
            NewId = "";
            _idTyped = false;
            NewTemplate = Templates[0];
            ResetNewAgentChoice();
            NewNameProblem = null;
            NewIdProblem = null;
            CreateProblem = null;
            IsEditing = false;
            IsCreating = true;
        }

        [RelayCommand]
        private void CancelCreate() => IsCreating = false;

        /// <summary>Creates DevTools\Sources\&lt;id&gt;\ from the chosen template, then writes the name into its manifest.</summary>
        [RelayCommand]
        private async Task Create()
        {
            var id = (NewId ?? "").Trim();
            var name = (NewName ?? "").Trim();
            var path = _projects.NewProjectPath(id);

            NewNameProblem = ProjectManifest.Problem(ProjectManifest.Name, name);
            NewIdProblem = ProjectManifest.Problem(ProjectManifest.Id, id);
            if (NewIdProblem == null && Directory.Exists(path) && Directory.EnumerateFileSystemEntries(path).Any())
                NewIdProblem = $"There is already a folder called {id} in {DevTools.Sources}.";
            CreateProblem = _twllink.Exists ? null : "twllink.exe is missing - see the note above.";
            if (NewNameProblem != null || NewIdProblem != null || CreateProblem != null)
                return;

            Busy = true;
            try
            {
                Directory.CreateDirectory(DevTools.Sources);
                var result = await _twllink.NewAsync((NewTemplate ?? Templates[0]).Key, path);
                if (!result.Success)
                {
                    CreateProblem = result.ToolMessage;
                    return;
                }

                // twllink names the project after its folder; the name the user typed replaces that, and a
                // description replaces the template's placeholder when one was given.
                var fields = new Dictionary<string, string> { [ProjectManifest.Name] = name };
                if (!string.IsNullOrWhiteSpace(NewDescription) && ProjectManifest.Problem(ProjectManifest.Description, NewDescription) == null)
                    fields[ProjectManifest.Description] = NewDescription;
                ProjectManifest.Update(path, fields);
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException || exception is InvalidDataException || exception is System.Text.Json.JsonException)
            {
                CreateProblem = "Could not create the project: " + exception.Message;
                return;
            }
            finally
            {
                Busy = false;
            }

            // The project exists whatever happens here; a kit problem is reported, not a failed Create.
            await SetUpNewProjectAsync(path, name);

            IsCreating = false;
            _projects.Remember(path);
            Reload();
            Selected = Projects.FirstOrDefault(p => string.Equals(Normalize(p.Root), Normalize(path), StringComparison.OrdinalIgnoreCase)) ?? Selected;
        }

        // --- editing the manifest -------------------------------------------------------------------------

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(SaveEditCommand))]
        private string editName;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(SaveEditCommand))]
        [NotifyPropertyChangedFor(nameof(IdChangeWarning))]
        private string editId;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(SaveEditCommand))]
        private string editAuthor;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(SaveEditCommand))]
        private string editVersion;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(SaveEditCommand))]
        private string editDescription;

        public string EditNameProblem => ProjectManifest.Problem(ProjectManifest.Name, EditName);
        public string EditIdProblem => ProjectManifest.Problem(ProjectManifest.Id, EditId);
        public string EditAuthorProblem => ProjectManifest.Problem(ProjectManifest.Author, EditAuthor);
        public string EditVersionProblem => ProjectManifest.Problem(ProjectManifest.Version, EditVersion);
        public string EditDescriptionProblem => ProjectManifest.Problem(ProjectManifest.Description, EditDescription);

        /// <summary>What a new id means for the copy already in the game - said before saving, not after.</summary>
        public string IdChangeWarning =>
            Selected?.Project.Id != null && EditIdProblem == null && !string.Equals(EditId, Selected.Project.Id, StringComparison.Ordinal)
                ? $"A new id is a new script: the next install writes {EditId}.lua, and {Selected.Project.Id}.lua already in the game stays there "
                  + "as a separate script until you delete it (Scripts tab → Show in Explorer)."
                : null;

        partial void OnEditNameChanged(string value) => OnPropertyChanged(nameof(EditNameProblem));
        partial void OnEditIdChanged(string value) => OnPropertyChanged(nameof(EditIdProblem));
        partial void OnEditAuthorChanged(string value) => OnPropertyChanged(nameof(EditAuthorProblem));
        partial void OnEditVersionChanged(string value) => OnPropertyChanged(nameof(EditVersionProblem));
        partial void OnEditDescriptionChanged(string value)
        {
            var line = OneLine(value);
            if (line != value)
            {
                EditDescription = line;
                return;
            }
            OnPropertyChanged(nameof(EditDescriptionProblem));
        }

        partial void OnNewDescriptionChanged(string value)
        {
            var line = OneLine(value);
            if (line != value)
                NewDescription = line;
        }

        /// <summary>
        /// A description is one line in the manifest and in the script's header - the plugin reads the header line by
        /// line, and twllink refuses a line break there. The field wraps it over several lines for reading, and Enter
        /// does not add one; a line break pasted in becomes a space, so pasting a paragraph just works.
        /// </summary>
        private static string OneLine(string value)
        {
            if (value == null || value.IndexOfAny(new[] { '\r', '\n', '\t' }) < 0)
                return value;
            return System.Text.RegularExpressions.Regex.Replace(value, @"[ \t]*[\r\n\t]+[ \t]*", " ");
        }

        [RelayCommand(CanExecute = nameof(CanStartEdit))]
        private void StartEdit()
        {
            var project = Selected.Project;
            EditName = project.Name ?? "";
            EditId = project.Id ?? "";
            EditAuthor = project.Author ?? "";
            EditVersion = project.Version ?? "";
            EditDescription = project.Description ?? "";
            IsEditing = true;
        }

        private bool CanStartEdit() => Selected != null && !Selected.IsMissing;

        [RelayCommand]
        private void CancelEdit() => IsEditing = false;

        [RelayCommand(CanExecute = nameof(CanSaveEdit))]
        private void SaveEdit()
        {
            var row = Selected;
            if (row == null)
                return;

            var project = row.Project;
            var changes = new Dictionary<string, string>();
            void Changed(string field, string value, string was)
            {
                if (!string.Equals((value ?? "").Trim(), was ?? "", StringComparison.Ordinal))
                    changes[field] = value;
            }
            Changed(ProjectManifest.Id, EditId, project.Id);
            Changed(ProjectManifest.Name, EditName, project.Name);
            Changed(ProjectManifest.Author, EditAuthor, project.Author);
            Changed(ProjectManifest.Version, EditVersion, project.Version);
            Changed(ProjectManifest.Description, EditDescription, project.Description);

            if (changes.Count > 0)
            {
                try
                {
                    ProjectManifest.Update(row.Root, changes);
                }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException || exception is InvalidDataException || exception is System.Text.Json.JsonException)
                {
                    ApplicationNotificationManager.Manager.ShowError("Script Dev",
                        $"Could not save {ScriptProjects.ManifestFileName}: {exception.Message}");
                    return;
                }
            }

            IsEditing = false;
            Reload();
        }

        private bool CanSaveEdit() =>
            EditNameProblem == null && EditIdProblem == null && EditAuthorProblem == null && EditVersionProblem == null && EditDescriptionProblem == null;

        // --- deleting ---------------------------------------------------------------------------------------

        /// <summary>
        /// The project folder goes to the Recycle Bin - not gone for good: it is the user's work, and a button next to
        /// "Build" is one misclick away. What was installed into the game stays; that is the Scripts tab's to remove.
        /// </summary>
        [RelayCommand(CanExecute = nameof(CanOpenProjectFolder))]
        private async Task DeleteProject()
        {
            var row = Selected;
            if (row == null)
                return;

            var confirmed = await ApplicationNotificationManager.Manager.AskForAction("Delete project",
                $"The folder {row.Root} goes to the Recycle Bin, with everything in it. "
                + (row.ScriptFileName != null ? $"{row.ScriptFileName} installed in the game stays - remove it in the Scripts tab if you want it gone too." : ""));
            if (!confirmed)
                return;

            _watcher.Stop();
            var error = await ShellActions.RecycleFolderAsync(row.Root);
            if (error != null)
            {
                ApplicationNotificationManager.Manager.ShowError("Script Dev", $"Could not delete {row.Root}: {error}");
                Watch();
                return;
            }

            _projects.Forget(row.Root);
            Selected = null;
            Reload();
        }

        /// <summary>
        /// Adds a project from anywhere to the list, the way an editor's "Open Folder" does: the files stay where
        /// they are, the folder is remembered in DevTools\recent-projects.txt.
        /// </summary>
        [RelayCommand]
        private async Task OpenExisting()
        {
            var folder = await FileDialogService.OpenFolderAsync("Open a script project (a folder with twproject.json)");
            if (folder == null)
                return;

            if (!_projects.Remember(folder))
            {
                ApplicationNotificationManager.Manager.ShowWarning("Script Dev",
                    $"{folder} has no {ScriptProjects.ManifestFileName}, so it is not a project. Create one with New project, or run twllink --new in it.");
                return;
            }

            Reload();
            Selected = Projects.FirstOrDefault(p => string.Equals(Normalize(p.Root), Normalize(folder), StringComparison.OrdinalIgnoreCase)) ?? Selected;
        }

        [RelayCommand(CanExecute = nameof(CanForget))]
        private void Forget()
        {
            if (Selected == null)
                return;
            _projects.Forget(Selected.Root);
            Reload();
        }

        private bool CanForget() => Selected != null && Selected.IsRemembered;

        [RelayCommand(CanExecute = nameof(CanOpenProjectFolder))]
        private void OpenProjectFolder() => ShellActions.OpenFolder("Script Dev", Selected.Root);

        private bool CanOpenProjectFolder() => Selected != null && !Selected.IsMissing;

        // --- building -------------------------------------------------------------------------------------

        [RelayCommand(CanExecute = nameof(CanBuild))]
        private Task Build() => BuildAsync(Selected, install: false);

        [RelayCommand(CanExecute = nameof(CanBuild))]
        private async Task Install()
        {
            var row = Selected;
            if (row == null || !await ConfirmCollisionIfNeededAsync(row))
                return;
            await BuildAsync(row, install: true);
        }

        private bool CanBuild() => Selected != null && !Selected.IsMissing && !Busy;

        private async Task BuildAsync(ScriptProjectRowViewModel row, bool install)
        {
            if (row == null)
                return;
            if (!_twllink.Exists)
            {
                UpdateNotice();
                return;
            }

            var game = install ? PluginService.EngineDirectory : null;
            if (install && game == null)
            {
                ApplicationNotificationManager.Manager.ShowWarning("Script Dev", "Audiosurf's folder is unknown - set the game textures path in Settings.");
                return;
            }

            _watcher.WorkStarted();
            Busy = true;
            row.ShowBuilding(install);
            try
            {
                var result = await _twllink.BuildAsync(row.Root, game);

                // Installed into a game that is running and connected: have it run the new build now (§8.10),
                // on a rebuild-on-save too. A script that was off stays off - the plugin only re-reads its header.
                var reloaded = result.Success && result.Build?.Installed != null && ScriptsOverlayBridge.IsLive;
                if (reloaded)
                    ScriptsOverlayBridge.Reload(new[] { Path.GetFileName(result.Build.Installed) });

                row.ShowResult(result, DateTime.Now, reloaded);
                if (result.Build != null)
                    Logger.Log("Script Dev", $"{row.Root}: exit {result.ExitCode}, {result.Build.Errors} error(s), {result.Build.Warnings} warning(s)");
                else
                    Logger.Log("Script Dev", $"{row.Root}: exit {result.ExitCode}: {result.ToolMessage}");
            }
            finally
            {
                Busy = false;
                // Changes that arrived during the build are the build after it.
                _watcher.WorkFinished();
            }
        }

        /// <summary>
        /// The one question installing can need (§8.6): the project's script has the name of a bundled one.
        /// Asked once per project per session; true when there is no collision or the user said go ahead.
        /// </summary>
        private async Task<bool> ConfirmCollisionIfNeededAsync(ScriptProjectRowViewModel row)
        {
            var file = row.ScriptFileName;
            if (file == null || _collisionConfirmed.Contains(row.Root))
                return true;

            var engine = PluginService.EngineDirectory;
            if (!await Task.Run(() => GameScripts.IsShipped(engine, PluginService.PayloadDirectory, file)))
                return true;

            var go = await ApplicationNotificationManager.Manager.AskForAction("Name taken by a bundled script",
                $"The name {Path.GetFileNameWithoutExtension(file)} is the name of a script that comes with Tweaker. Installing yours replaces it in the game, "
                + "and the next plugin update will move yours aside as a .old copy and put the bundled one back. Install anyway?");
            if (go)
                _collisionConfirmed.Add(row.Root);
            return go;
        }

        // --- editors (С4b) ---------------------------------------------------------------------------------

        [RelayCommand(CanExecute = nameof(CanOpenInEditor))]
        private Task OpenInEditor() => OpenAsync(Selected, null, 0, 0);

        private bool CanOpenInEditor() => Editor.SelectedEditor != null && Selected != null && !Selected.IsMissing;

        /// <summary>A diagnostic, double-clicked: its file at its line, in the chosen editor.</summary>
        [RelayCommand]
        private Task OpenDiagnostic(DiagnosticRowViewModel diagnostic)
        {
            if (diagnostic == null || Selected == null)
                return Task.CompletedTask;
            var file = string.IsNullOrEmpty(diagnostic.Diagnostic.File) ? null : diagnostic.Diagnostic.File;
            return OpenAsync(Selected, file, diagnostic.Diagnostic.Line, diagnostic.Diagnostic.Column);
        }

        private Task OpenAsync(ScriptProjectRowViewModel row, string file, int line, int column)
        {
            if (row == null || row.IsMissing)
                return Task.CompletedTask;
            return Editor.OpenAsync(row.Root, file, line, column);
        }

        // --- rebuild on save --------------------------------------------------------------------------------

        private void Watch()
        {
            _watcher.Stop();
            var row = Selected;
            if (RebuildOnSave && row != null && !row.IsMissing)
                _watcher.Start(row.Root);
        }

        private async Task AutoBuildAsync()
        {
            var row = Selected;
            if (!RebuildOnSave || row == null)
                return;
            await BuildAsync(row, install: InstallOnRebuild && CollisionConfirmedOrFree(row));
        }

        /// <summary>
        /// Auto-install never asks - the question was asked when the switch went on. A project that became a
        /// collision since (renamed id) builds without installing until the user installs it by hand once.
        /// </summary>
        private bool CollisionConfirmedOrFree(ScriptProjectRowViewModel row)
        {
            var file = row.ScriptFileName;
            return file == null
                   || _collisionConfirmed.Contains(row.Root)
                   || !GameScripts.IsShipped(PluginService.EngineDirectory, PluginService.PayloadDirectory, file);
        }

        private static string Normalize(string path) => Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
    }
}
