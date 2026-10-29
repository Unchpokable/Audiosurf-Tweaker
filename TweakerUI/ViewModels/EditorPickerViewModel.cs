using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using TweakerCore.DevProjects;
using TweakerUI.Core;

namespace TweakerUI.ViewModels
{
    /// <summary>
    /// The Editor card of a developer tab - Script Dev, Skyboxes (Docs/Internal/script-manager.md §6, §8.3;
    /// sky-manager-roadmap.md §4.1): the installed editors, the chosen one, and opening a project or a place in it.
    ///
    /// Each tab has its own, with its own <see cref="EditorEnvironment"/> - Script Dev puts twllink and cgrpy on the
    /// editor's PATH, Skyboxes adds nothing. The choice is one setting for both (SettingsProvider.ScriptEditor): the
    /// tab shown last re-reads it when it looks for editors again.
    /// </summary>
    public partial class EditorPickerViewModel : ViewModelBase, IDisposable
    {
        private readonly string _owner;
        private readonly Func<EditorEnvironment> _environment;
        private readonly string _toolsOnPath;

        /// <summary>The editor this tab started, per editor - its window still being up means it has our environment.</summary>
        private readonly Dictionary<string, Process> _launched = new Dictionary<string, Process>();

        /// <summary>Editors the user was already warned about this session and chose to open anyway.</summary>
        private readonly HashSet<string> _warnedRunning = new HashSet<string>();

        private bool _restoring;

        /// <param name="owner">The tab, for notifications and the log.</param>
        /// <param name="environment">What the editor's process gets on top of Tweaker's, asked at each launch.</param>
        /// <param name="hint">Under the list, what opening from here gives; null for nothing.</param>
        /// <param name="toolsOnPath">
        /// Named in the "already running" warning ("twllink"): what the editor's terminal would miss. Only asked when
        /// the environment is not empty - an editor started with nothing added misses nothing.
        /// </param>
        public EditorPickerViewModel(string owner, Func<EditorEnvironment> environment, string hint, string toolsOnPath)
        {
            _owner = owner;
            _environment = environment;
            _toolsOnPath = toolsOnPath;
            Hint = hint;
        }

        public ObservableCollection<ProjectEditor> Editors { get; } = new ObservableCollection<ProjectEditor>();

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(OpenInEditorText))]
        private ProjectEditor selectedEditor;

        [ObservableProperty]
        private bool hasEditors;

        public string Hint { get; }

        public bool ShowHint => HasEditors && !string.IsNullOrEmpty(Hint);

        partial void OnHasEditorsChanged(bool value) => OnPropertyChanged(nameof(ShowHint));

        public string OpenInEditorText => SelectedEditor == null ? "Open in editor" : "Open in " + SelectedEditor.Name;

        /// <summary>Looked for every time the tab is shown: an editor installed while Tweaker runs shows up without a restart.</summary>
        public async Task DetectAsync()
        {
            var found = await Task.Run(() => TweakerCore.DevProjects.Editors.Detect(EditorSearch.FromSystem()));

            _restoring = true;
            try
            {
                // The setting first: the other developer tab may have changed it since this one last looked.
                var keep = string.IsNullOrEmpty(SettingsProvider.ScriptEditor) ? SelectedEditor?.Id : SettingsProvider.ScriptEditor;
                Editors.Clear();
                foreach (var editor in found)
                    Editors.Add(editor);
                HasEditors = Editors.Count > 0;
                SelectedEditor = Editors.FirstOrDefault(e => e.Id == keep) ?? Editors.FirstOrDefault();
            }
            finally
            {
                _restoring = false;
            }
        }

        partial void OnSelectedEditorChanged(ProjectEditor value)
        {
            if (_restoring || value == null || value.Id == SettingsProvider.ScriptEditor)
                return;
            SettingsProvider.ScriptEditor = value.Id;
            ConfigurationManager.RewriteSettings();
        }

        /// <summary>
        /// Opens <paramref name="project"/> in the chosen editor, or one of its files at a line
        /// (<paramref name="file"/> relative to the project, '/' separators; null for the project alone; 0 for an
        /// unknown line or column).
        /// </summary>
        public async Task OpenAsync(string project, string file, int line, int column)
        {
            var editor = SelectedEditor;
            if (editor == null)
            {
                ApplicationNotificationManager.Manager.ShowInformation(_owner,
                    "No editor to open it in. Install Visual Studio Code, Cursor, Sublime Text, Notepad++, Vim or Neovim, then come back to this tab.");
                return;
            }

            var environment = _environment() ?? EditorEnvironment.None;
            if (!await ConfirmRunningEditorAsync(editor, environment))
                return;

            var info = TweakerCore.DevProjects.Editors.StartInfo(editor, project, file, line, column, environment);
            var error = TweakerCore.DevProjects.Editors.Launch(info, out var process);
            if (error != null)
            {
                ApplicationNotificationManager.Manager.ShowError(_owner, $"Could not start {editor.Name}: {error}");
                return;
            }

            // Kept only while it is the editor's main process. A launch that handed its arguments to a running
            // instance exits within moments, and that instance is not ours.
            if (_launched.TryGetValue(editor.Id, out var previous) && !IsAlive(previous))
            {
                previous.Dispose();
                _launched.Remove(editor.Id);
            }
            if (!_launched.ContainsKey(editor.Id))
                _launched[editor.Id] = process;
            else
                process.Dispose();
        }

        public void Dispose()
        {
            foreach (var process in _launched.Values)
                process.Dispose();
            _launched.Clear();
        }

        /// <summary>
        /// The owner's warning (script-manager.md §8.3): an editor that keeps one process will hand the project to the
        /// instance that is already running, and that instance's terminal has none of what this launch would add. Not
        /// asked when nothing is added, when the running instance is the one this tab started, and not asked twice for
        /// the same editor in a session.
        /// </summary>
        private async Task<bool> ConfirmRunningEditorAsync(ProjectEditor editor, EditorEnvironment environment)
        {
            if (environment.IsEmpty || !editor.KeepsOneProcessWithTerminal || _warnedRunning.Contains(editor.Id))
                return true;
            if (_launched.TryGetValue(editor.Id, out var ours) && IsAlive(ours))
                return true;
            if (!await Task.Run(editor.IsRunning))
                return true;

            var choice = await ApplicationNotificationManager.Manager.AskForChoice($"{editor.Name} is already running",
                $"{editor.Name} is already running, so it keeps the PATH it was started with - {_toolsOnPath} may not be available "
                + $"from its terminal. If you need {_toolsOnPath} there, close {editor.Name} and open the project from here.",
                "Open anyway", "Cancel");
            if (choice != 0)
                return false;
            _warnedRunning.Add(editor.Id);
            return true;
        }

        private static bool IsAlive(Process process)
        {
            try
            {
                return !process.HasExited;
            }
            catch (InvalidOperationException)
            {
                return false;
            }
        }
    }
}
