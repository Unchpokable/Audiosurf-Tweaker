using System;
using System.Collections.ObjectModel;
using System.IO;
using CommunityToolkit.Mvvm.ComponentModel;
using TweakerCore.Scripting;

namespace TweakerUI.Models
{
    /// <summary>What the last build of a project came to, for its status line.</summary>
    public enum BuildOutcome
    {
        None,
        Building,
        Succeeded,
        Failed,
        ToolFailed
    }

    /// <summary>
    /// One project in the Script Dev tab, with its build state. The state lives here rather than on the tab so
    /// that switching between projects shows each one's last build instead of wiping it. Everything that
    /// runs twllink is the tab's (<see cref="ViewModels.ScriptDevViewModel"/>); this only holds and shows.
    /// </summary>
    public partial class ScriptProjectRowViewModel : ObservableObject
    {
        private ScriptProject _project;

        public ScriptProjectRowViewModel(ScriptProject project)
        {
            Update(project);
        }

        public ScriptProject Project => _project;

        public string Root => _project.Root;

        public string Title => _project.Name ?? _project.Id ?? Path.GetFileName(Path.TrimEndingDirectorySeparator(_project.Root));

        /// <summary>"my_hud · v0.1.0"; the folder name when the manifest did not read.</summary>
        public string Subtitle
        {
            get
            {
                if (_project.Id == null)
                    return Path.GetFileName(Path.TrimEndingDirectorySeparator(_project.Root));
                return _project.Version == null ? _project.Id : _project.Id + "  ·  v" + _project.Version;
            }
        }

        public string Description => _project.Description ?? "";

        public bool HasDescription => !string.IsNullOrEmpty(_project.Description);

        public bool IsMissing => _project.IsMissing;

        /// <summary>Opened from elsewhere and remembered - the only kind "forget" applies to.</summary>
        public bool IsRemembered => !_project.InSources;

        public string Problem => _project.ManifestProblem;

        public bool HasProblem => _project.ManifestProblem != null && !_project.IsMissing;

        /// <summary>For the list, where the whole parser message does not fit; the details panel shows <see cref="Problem"/>.</summary>
        public string ShortProblem => ScriptProjects.ManifestFileName + " cannot be read";

        /// <summary>"my_hud.lua" - what it becomes in the game's Scripts folder; null without an id.</summary>
        public string ScriptFileName => _project.ScriptFileName;

        [ObservableProperty]
        private BuildOutcome outcome;

        [ObservableProperty]
        private string buildStatus;

        [ObservableProperty]
        private string buildDetail;

        public ObservableCollection<DiagnosticRowViewModel> Diagnostics { get; } = new ObservableCollection<DiagnosticRowViewModel>();

        [ObservableProperty]
        private bool hasDiagnostics;

        public bool IsBuilding => Outcome == BuildOutcome.Building;

        public bool Succeeded => Outcome == BuildOutcome.Succeeded;

        public bool Failed => Outcome == BuildOutcome.Failed || Outcome == BuildOutcome.ToolFailed;

        partial void OnOutcomeChanged(BuildOutcome value)
        {
            OnPropertyChanged(nameof(IsBuilding));
            OnPropertyChanged(nameof(Succeeded));
            OnPropertyChanged(nameof(Failed));
        }

        public void Update(ScriptProject project)
        {
            _project = project;
            OnPropertyChanged(nameof(Project));
            OnPropertyChanged(nameof(Title));
            OnPropertyChanged(nameof(Subtitle));
            OnPropertyChanged(nameof(Description));
            OnPropertyChanged(nameof(HasDescription));
            OnPropertyChanged(nameof(IsMissing));
            OnPropertyChanged(nameof(IsRemembered));
            OnPropertyChanged(nameof(Problem));
            OnPropertyChanged(nameof(HasProblem));
            OnPropertyChanged(nameof(ScriptFileName));
        }

        /// <summary>
        /// Shows a finished build: the status line, what it wrote, and every diagnostic. <paramref name="reloaded"/>:
        /// the install was followed by a reload in the running game (С5b).
        /// </summary>
        public void ShowResult(TwllinkResult result, DateTime when, bool reloaded = false)
        {
            Diagnostics.Clear();
            var time = when.ToString("HH:mm:ss");

            if (result.Build == null)
            {
                Outcome = BuildOutcome.ToolFailed;
                BuildStatus = $"twllink could not build it ({time})";
                BuildDetail = result.ToolMessage;
                HasDiagnostics = false;
                return;
            }

            var build = result.Build;
            foreach (var diagnostic in build.Diagnostics)
                Diagnostics.Add(new DiagnosticRowViewModel(diagnostic));
            HasDiagnostics = Diagnostics.Count > 0;

            var warnings = build.Warnings == 0 ? "" : build.Warnings == 1 ? ", 1 warning" : $", {build.Warnings} warnings";
            if (result.Success)
            {
                Outcome = BuildOutcome.Succeeded;
                var modules = build.Modules.Count == 1 ? "1 module" : build.Modules.Count + " modules";
                BuildStatus = (build.Installed == null ? "Built" : reloaded ? "Built, installed and reloaded" : "Built and installed")
                              + $" at {time} - {modules}{warnings}";
                BuildDetail = build.Installed == null
                    ? "Written to " + build.Output
                    : "In the game: " + build.Installed
                      + (reloaded ? " - the running game was asked to reload it; how that went is in the Scripts tab and the overlay." : "");
            }
            else if (build.Output != null)
            {
                // Built, and then --install could not copy it: twllink's exit code 1 covers both.
                Outcome = BuildOutcome.Failed;
                BuildStatus = $"Built at {time}, but not installed{warnings}";
                BuildDetail = "Written to " + build.Output;
            }
            else
            {
                Outcome = BuildOutcome.Failed;
                var errors = build.Errors == 1 ? "1 error" : build.Errors + " errors";
                BuildStatus = $"Build failed at {time} - {errors}{warnings}";
                BuildDetail = null;
            }
        }

        public void ShowBuilding(bool install)
        {
            Outcome = BuildOutcome.Building;
            BuildStatus = install ? "Building and installing..." : "Building...";
        }
    }
}
