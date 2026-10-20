using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerCore.Scripting;
using TweakerUI.Core;
using TweakerUI.Models;
using TweakerUI.Services;

namespace TweakerUI.ViewModels
{
    /// <summary>
    /// Script Dev's side of the AI agent kit (Docs/Internal/dev-bundle-roadmap.md §3.6): set a project up for Claude
    /// Code, Codex and Cursor from DevTools\AgentKit, keep it up to date, and say where it stands. The files and the
    /// rules about who owns which are TweakerCore.Scripting.AgentSetup's; this decides when, and shows the answer.
    ///
    /// Also uv, which CgrPy needs and the Tweaker does not ship (§2.2): a card saying how to get it when it is missing.
    /// </summary>
    public partial class ScriptDevViewModel
    {
        // --- the new-project form -----------------------------------------------------------------------------

        [ObservableProperty]
        private bool newSetUpAgents;

        [ObservableProperty]
        private bool newAgentClaude;

        [ObservableProperty]
        private bool newAgentCodex;

        [ObservableProperty]
        private bool newAgentCursor;

        [ObservableProperty]
        private bool newAgentBinaries;

        /// <summary>The form starts from what the user chose last time (§2.16).</summary>
        private void ResetNewAgentChoice()
        {
            var remembered = RememberedChoice();
            NewSetUpAgents = SettingsProvider.ScriptAgentSetup;
            NewAgentClaude = remembered.ClaudeCode;
            NewAgentCodex = remembered.Codex;
            NewAgentCursor = remembered.Cursor;
            NewAgentBinaries = remembered.Binaries;
        }

        private AgentChoice NewAgentChoice => new AgentChoice(NewAgentClaude, NewAgentCodex, NewAgentCursor, NewAgentBinaries);

        /// <summary>Right after twllink --new: the project exists, and the form asked for agents.</summary>
        private async Task SetUpNewProjectAsync(string path, string name)
        {
            SettingsProvider.ScriptAgentSetup = NewSetUpAgents;
            if (!NewSetUpAgents)
            {
                ConfigurationManager.RewriteSettings();
                return;
            }

            Remember(NewAgentChoice);
            await ApplyAgentsAsync(path, name, NewAgentChoice, replaceEdited: false);
        }

        // --- the selected project -----------------------------------------------------------------------------

        [ObservableProperty]
        private string agentStatus;

        [ObservableProperty]
        private string agentDetail;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(OpenAgentPanelCommand))]
        private bool agentKitAvailable;

        [ObservableProperty]
        private bool agentsSetUp;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(UpdateAgentsCommand))]
        private bool agentUpdateAvailable;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(ReplaceAgentEditsCommand))]
        private bool agentHasEdits;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(OpenAgentPanelCommand), nameof(UpdateAgentsCommand), nameof(ReplaceAgentEditsCommand), nameof(ApplyAgentsCommand))]
        private bool agentBusy;

        /// <summary>The choice of agents, open under the project's actions.</summary>
        [ObservableProperty]
        private bool isAgentPanelOpen;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(ApplyAgentsCommand))]
        private bool agentClaude;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(ApplyAgentsCommand))]
        private bool agentCodex;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(ApplyAgentsCommand))]
        private bool agentCursor;

        [ObservableProperty]
        private bool agentBinaries;

        public string AgentPanelTitle => AgentsSetUp ? "AI agents for this project" : "Set this project up for AI agents";

        public string AgentApplyText => AgentsSetUp ? "Apply" : "Set up";

        public string AgentSetupButtonText => AgentsSetUp ? "Agents…" : "Set up for AI agents";

        partial void OnAgentsSetUpChanged(bool value)
        {
            OnPropertyChanged(nameof(AgentPanelTitle));
            OnPropertyChanged(nameof(AgentApplyText));
            OnPropertyChanged(nameof(AgentSetupButtonText));
        }

        /// <summary>
        /// Where the selected project stands. Also rewrites its .tweaker\local.json when it is set up: the game or the
        /// Tweaker may have moved since it was last opened, and the skills read their paths from there.
        /// </summary>
        private async Task RefreshAgentsAsync()
        {
            var row = Selected;
            if (row == null || row.IsMissing)
            {
                AgentStatus = null;
                AgentDetail = null;
                AgentsSetUp = AgentUpdateAvailable = AgentHasEdits = false;
                return;
            }

            AgentKit kit = null;
            string problem = null;
            AgentSetupStatus status = null;
            await Task.Run(() =>
            {
                kit = AgentKit.Load(DevTools.AgentKit, out problem);
                if (kit == null)
                    return;
                try
                {
                    status = AgentSetup.Status(row.Root, kit, DevTools.Docs);
                    AgentSetup.WriteLocal(row.Root, PluginService.GameRootDirectory, DevTools.Root);
                }
                catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
                {
                    problem = "Could not read the project's agent setup: " + exception.Message;
                }
            });

            if (row != Selected)
                return; // another project was picked while this one was being looked at

            AgentKitAvailable = kit != null && status != null;
            AgentsSetUp = status != null && status.State != AgentSetupState.NotSetUp;
            AgentUpdateAvailable = status?.State == AgentSetupState.UpdateAvailable;
            AgentHasEdits = status != null && status.Edited.Count > 0;

            if (problem != null)
            {
                AgentStatus = problem;
                AgentDetail = null;
                return;
            }

            AgentStatus = status.State switch
            {
                AgentSetupState.NotSetUp => "Not set up for AI coding agents.",
                AgentSetupState.UpToDate => $"Set up for {Describe(status.Choice)} - up to date (kit {status.KitVersion}).",
                _ => status.InstalledKitVersion != status.KitVersion
                    ? $"Set up for {Describe(status.Choice)} - kit {status.KitVersion} is available (the project has {status.InstalledKitVersion})."
                    : $"Set up for {Describe(status.Choice)} - {Count(status.Pending.Count, "file")} to bring up to date."
            };
            AgentDetail = AgentHasEdits
                ? $"{Count(status.Edited.Count, "file")} you changed {(status.Edited.Count == 1 ? "is" : "are")} kept as you left {(status.Edited.Count == 1 ? "it" : "them")}: "
                  + string.Join(", ", status.Edited.Take(3)) + (status.Edited.Count > 3 ? ", …" : "")
                : null;
        }

        [RelayCommand(CanExecute = nameof(CanOpenAgentPanel))]
        private void OpenAgentPanel()
        {
            var choice = (Selected != null ? AgentSetup.ChoiceOf(Selected.Root) : null) ?? RememberedChoice();
            AgentClaude = choice.ClaudeCode;
            AgentCodex = choice.Codex;
            AgentCursor = choice.Cursor;
            AgentBinaries = choice.Binaries;
            IsAgentPanelOpen = true;
        }

        private bool CanOpenAgentPanel() => AgentKitAvailable && !AgentBusy;

        [RelayCommand]
        private void CancelAgents() => IsAgentPanelOpen = false;

        [RelayCommand(CanExecute = nameof(CanApplyAgents))]
        private async Task ApplyAgents()
        {
            var row = Selected;
            if (row == null)
                return;
            var choice = new AgentChoice(AgentClaude, AgentCodex, AgentCursor, AgentBinaries);
            Remember(choice);
            IsAgentPanelOpen = false;
            await ApplyAgentsAsync(row.Root, row.Project.Name, choice, replaceEdited: false);
        }

        /// <summary>At least one agent: with none, the files would be there for nobody.</summary>
        private bool CanApplyAgents() => (AgentClaude || AgentCodex || AgentCursor) && !AgentBusy;

        [RelayCommand(CanExecute = nameof(CanUpdateAgents))]
        private async Task UpdateAgents()
        {
            var row = Selected;
            var choice = row != null ? AgentSetup.ChoiceOf(row.Root) : null;
            if (choice != null)
                await ApplyAgentsAsync(row.Root, row.Project.Name, choice, replaceEdited: false);
        }

        private bool CanUpdateAgents() => AgentUpdateAvailable && !AgentBusy;

        /// <summary>The kit's version over the user's edits - asked first; their versions are kept as .old copies.</summary>
        [RelayCommand(CanExecute = nameof(CanReplaceAgentEdits))]
        private async Task ReplaceAgentEdits()
        {
            var row = Selected;
            var choice = row != null ? AgentSetup.ChoiceOf(row.Root) : null;
            if (choice == null)
                return;

            var go = await ApplicationNotificationManager.Manager.AskForAction("Use the kit's version",
                "The agent files you changed in this project get this Tweaker's version. Yours are not lost: each is kept next to it as a .old copy.");
            if (go)
                await ApplyAgentsAsync(row.Root, row.Project.Name, choice, replaceEdited: true);
        }

        private bool CanReplaceAgentEdits() => AgentHasEdits && !AgentBusy;

        private async Task ApplyAgentsAsync(string root, string name, AgentChoice choice, bool replaceEdited)
        {
            AgentBusy = true;
            try
            {
                string problem = null;
                AgentSetupReport report = null;
                await Task.Run(() =>
                {
                    var kit = AgentKit.Load(DevTools.AgentKit, out problem);
                    if (kit != null)
                        report = AgentSetup.Apply(root, name, choice, kit, DevTools.Docs, PluginService.GameRootDirectory, DevTools.Root, replaceEdited);
                });

                if (problem != null)
                    ApplicationNotificationManager.Manager.ShowError("Script Dev", problem);
                else if (!report.Success)
                    ApplicationNotificationManager.Manager.ShowError("Script Dev", "Setting the project up for AI agents was not complete: " + string.Join("; ", report.Errors.Take(3)));
                else
                    Logger.Log("Script Dev", $"{root}: agents {choice.AgentList}{(choice.Binaries ? "+binaries" : "")} - {report.Added.Count} added, "
                                             + $"{report.Updated.Count} updated, {report.Removed.Count} removed, {report.KeptEdited.Count} kept, {report.BackedUp.Count} backed up");
            }
            finally
            {
                AgentBusy = false;
            }

            await RefreshAgentsAsync();
        }

        /// <summary>Nothing remembered is the default, not "no agents": a choice of none can never be applied, so it is never saved.</summary>
        private static AgentChoice RememberedChoice() =>
            string.IsNullOrWhiteSpace(SettingsProvider.ScriptAgents)
                ? AgentChoice.Default with { Binaries = SettingsProvider.ScriptAgentBinaries }
                : AgentChoice.FromList(SettingsProvider.ScriptAgents, SettingsProvider.ScriptAgentBinaries);

        private static void Remember(AgentChoice choice)
        {
            SettingsProvider.ScriptAgents = choice.AgentList;
            SettingsProvider.ScriptAgentBinaries = choice.Binaries;
            ConfigurationManager.RewriteSettings();
        }

        private static string Describe(AgentChoice choice)
        {
            var names = new List<string>();
            if (choice.ClaudeCode)
                names.Add("Claude Code");
            if (choice.Codex)
                names.Add("Codex");
            if (choice.Cursor)
                names.Add("Cursor");
            var agents = names.Count switch
            {
                0 => "no agent",
                1 => names[0],
                _ => string.Join(", ", names.Take(names.Count - 1)) + " and " + names[^1]
            };
            return choice.Binaries ? agents + ", with the binaries skill" : agents;
        }

        private static string Count(int n, string noun) => n == 1 ? "1 " + noun : $"{n} {noun}s";

        // --- uv ---------------------------------------------------------------------------------------------

        [ObservableProperty]
        private bool uvNotice;

        [ObservableProperty]
        private string uvMessage;

        public string UvInstallCommand => Uv.InstallCommand;

        /// <summary>Looked at every time the tab is shown: installing uv while the Tweaker runs is the usual fix.</summary>
        private async Task DetectUvAsync()
        {
            var state = await Task.Run(Uv.LocateOnSystem);
            UvNotice = state != UvState.Ready;
            UvMessage = state switch
            {
                UvState.NeedsRestart => "uv is installed, but after this Tweaker was started - an editor opened from here would not find it. "
                                        + "Restart the Tweaker, and cgrpy works in the editors it opens.",
                UvState.Missing => "cgrpy - the tools the game was taken apart with - needs uv, and uv is not installed. "
                                   + "The CgrPy README says so in its very first section. Install it, then restart the Tweaker:",
                _ => null
            };
        }
    }
}
