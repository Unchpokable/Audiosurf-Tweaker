using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerCore.PluginInstall;
using TweakerCore.Scripting;
using TweakerUI.Core;
using TweakerUI.Models;

namespace TweakerUI.ViewModels
{
    /// <summary>
    /// The Scripts tab (Docs/Internal/script-manager.md §5): the scripts in the game's
    /// engine\TweakerStuff\Scripts\, bundled and the user's own, switched on and off, and the bundled ones put
    /// back. The list comes from files (TweakerCore.Scripting); while the game is connected, the plugin's word
    /// about each script (ScriptsOverlayBridge) is laid over it.
    ///
    /// Switching has three modes, because scripts.cfg is the plugin's while the game runs (it reads the file at
    /// start and rewrites it on every toggle):
    ///  - no game: the tab writes scripts.cfg itself;
    ///  - the game, connected: the switch asks the plugin (SCRIPT_SET), shows the new position at once and waits up
    ///    to 5 s for the plugin to confirm - without an answer it springs back;
    ///  - the game, not connected (syncing off, or the plugin did not answer): locked, and the tab says why.
    ///
    /// Live only while visible: <see cref="SetActive"/> starts the folder watchers and the game-running poll
    /// and stops them again, so a tab nobody is looking at costs nothing.
    /// </summary>
    public partial class ScriptsViewModel : ViewModelBase, IDisposable
    {
        /// <summary>How often the tab checks whether Audiosurf started or stopped, while it is on screen.</summary>
        private static readonly TimeSpan GamePollInterval = TimeSpan.FromSeconds(2);

        /// <summary>Quiet time after the last file event before rescanning - one install writes several files.</summary>
        private static readonly TimeSpan RescanSettle = TimeSpan.FromMilliseconds(300);

        private readonly DispatcherTimer _gamePoll;
        private readonly DispatcherTimer _rescan;
        private readonly List<FileSystemWatcher> _watchers = new List<FileSystemWatcher>();
        private string _watchedEngine;
        private bool _active;
        private int _scanGeneration;

        public ScriptsViewModel()
        {
            _gamePoll = new DispatcherTimer { Interval = GamePollInterval };
            _gamePoll.Tick += (_, _) => UpdateGameRunning();
            _rescan = new DispatcherTimer { Interval = RescanSettle };
            _rescan.Tick += (_, _) =>
            {
                _rescan.Stop();
                _ = RefreshAsync();
            };

            // Not only while visible: the plugin sends the whole list once, at the handshake, and a tab opened later
            // still has to show it. Laying it over the rows is cheap, and with no rows there is nothing to lay.
            ScriptsOverlayBridge.Changed += (_, _) => ApplyLive();
            ScriptsOverlayBridge.RequestTimedOut += (_, file) =>
                ApplicationNotificationManager.Manager.ShowWarning("Scripts",
                    $"The game did not confirm switching {file}. The switch is back where it was - try again, or use the game's overlay.");
            Live = ScriptsOverlayBridge.IsLive;
        }

        /// <summary>Connected to the plugin in the running game: switches go to it, and the rows show its word.</summary>
        [ObservableProperty]
        private bool live;

        public ObservableCollection<ScriptRowViewModel> Scripts { get; } = new ObservableCollection<ScriptRowViewModel>();

        /// <summary>Shown instead of the list when there is no list to show (no game folder, empty folder).</summary>
        [ObservableProperty]
        private string emptyText;

        [ObservableProperty]
        private bool isEmpty = true;

        /// <summary>The tinted banner above the list: why the switches are locked, or why scripts do not run.</summary>
        [ObservableProperty]
        private string notice;

        [ObservableProperty]
        private bool hasNotice;

        [ObservableProperty]
        private bool gameRunning;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(OpenFolderCommand))]
        private bool hasScriptsFolder;

        [ObservableProperty]
        [NotifyCanExecuteChangedFor(nameof(RestoreAllCommand))]
        private bool anyRestorable;

        [ObservableProperty]
        private string summary;

        /// <summary>Called by MainWindowViewModel when the tab is shown or left.</summary>
        public void SetActive(bool active)
        {
            if (_active == active)
                return;
            _active = active;

            if (active)
            {
                UpdateGameRunning();
                _gamePoll.Start();
                _ = RefreshAsync();
            }
            else
            {
                _gamePoll.Stop();
                _rescan.Stop();
                StopWatching();
            }
        }

        /// <summary>
        /// Reads the folder again - and, with the game connected, asks the plugin to do the same: the one place a
        /// user asks for a rescan by hand (script-manager.md §8.9), so the two are one button rather than two that look alike.
        /// </summary>
        [RelayCommand]
        private Task Refresh()
        {
            ScriptsOverlayBridge.Rescan();
            return RefreshAsync();
        }

        [RelayCommand(CanExecute = nameof(HasScriptsFolder))]
        private void OpenFolder()
        {
            var engine = PluginService.EngineDirectory;
            if (engine == null)
                return;

            try
            {
                using (Process.Start(new ProcessStartInfo(PluginInstallation.ScriptsPath(engine)) { UseShellExecute = true }))
                {
                }
            }
            catch (Exception exception) when (exception is System.ComponentModel.Win32Exception || exception is InvalidOperationException)
            {
                Logger.Log("Scripts", "cannot open the Scripts folder: " + exception.Message);
            }
        }

        [RelayCommand(CanExecute = nameof(AnyRestorable))]
        private Task RestoreAll() => RestoreAsync(null);

        public void Dispose()
        {
            _gamePoll.Stop();
            _rescan.Stop();
            StopWatching();
        }

        // --- scanning -------------------------------------------------------------------------------------

        private async Task RefreshAsync()
        {
            var generation = ++_scanGeneration;
            var engine = PluginService.EngineDirectory;
            var payload = PluginService.PayloadDirectory;

            var scripts = engine == null
                ? Array.Empty<GameScript>()
                : await Task.Run(() => GameScripts.Scan(engine, payload));

            // A newer refresh started while this one was reading the disk; its answer is the one to show.
            if (generation != _scanGeneration)
                return;

            Watch(engine);
            HasScriptsFolder = engine != null && Directory.Exists(PluginInstallation.ScriptsPath(engine));
            Merge(scripts);
            AnyRestorable = Scripts.Any(row => row.CanRestore);

            if (engine == null)
                EmptyText = "Audiosurf's folder is unknown. Set the game textures path in Settings, and the scripts in the game show up here.";
            else if (Scripts.Count == 0)
                EmptyText = "No scripts in the game yet. Scripts go into engine\\TweakerStuff\\Scripts\\ - install the plugin in Settings to get the bundled ones.";
            IsEmpty = Scripts.Count == 0;

            UpdateSummary();
            UpdateNotice();
        }

        private void UpdateSummary()
        {
            var enabled = Scripts.Count(row => row.Enabled && !row.IsMissing);
            var present = Scripts.Count(row => !row.IsMissing);
            Summary = present == 0 ? "" : $"{enabled} of {present} enabled" + (Live ? "  ·  live from the game" : "");
        }

        /// <summary>The switch can move: never for a script that is not there; with the game up, only through the plugin.</summary>
        private bool CanSwitch(ScriptRowViewModel row) => !row.IsMissing && (Live || !GameRunning);

        private void ApplyLive(ScriptRowViewModel row)
        {
            row.ApplyLive(ScriptsOverlayBridge.StateOf(row.FileName), ScriptsOverlayBridge.PendingOf(row.FileName));
            row.CanToggle = CanSwitch(row);
        }

        /// <summary>The plugin said something, a switch was confirmed or dropped, or the link came or went.</summary>
        private void ApplyLive()
        {
            Live = ScriptsOverlayBridge.IsLive;
            foreach (var row in Scripts)
                ApplyLive(row);
            UpdateSummary();
            UpdateNotice();
        }

        /// <summary>Updates the list in place: rows keep their identity, so the view keeps its scroll and hover.</summary>
        private void Merge(IReadOnlyList<GameScript> scripts)
        {
            for (var i = 0; i < scripts.Count; i++)
            {
                var script = scripts[i];
                var existing = -1;
                for (var j = i; j < Scripts.Count; j++)
                {
                    if (string.Equals(Scripts[j].FileName, script.FileName, StringComparison.Ordinal))
                    {
                        existing = j;
                        break;
                    }
                }

                if (existing < 0)
                {
                    Scripts.Insert(i, new ScriptRowViewModel(script, TrySetEnabled, RestoreAsync));
                }
                else
                {
                    if (existing != i)
                        Scripts.Move(existing, i);
                    Scripts[i].Update(script);
                }
                ApplyLive(Scripts[i]);
            }

            while (Scripts.Count > scripts.Count)
                Scripts.RemoveAt(Scripts.Count - 1);
        }

        private void UpdateGameRunning()
        {
            var running = GameProcessService.IsRunning;
            if (running == GameRunning)
                return;

            GameRunning = running;
            foreach (var row in Scripts)
                row.CanToggle = CanSwitch(row);
            UpdateNotice();

            // The game rewrites scripts.cfg on its way through a session; what it left is worth a fresh look.
            if (!running)
                _ = RefreshAsync();
        }

        private void UpdateNotice()
        {
            var engine = PluginService.EngineDirectory;
            var lines = new List<string>();

            if (engine != null && !PluginInstallation.IsInstalled(engine))
                lines.Add("The plugin is not installed in this game, so none of these scripts run. Install it in Settings.");
            if (engine != null && GameRunning && !Live)
                lines.Add("Audiosurf is running, but Tweaker is not connected to its plugin (overlay sync is off in Settings, or the plugin "
                          + "has not answered). While the game runs, the plugin keeps the list of enabled scripts - switch them in the "
                          + "game's overlay (Scripts tab), or here once the game is closed or connected.");

            Notice = lines.Count == 0 ? null : string.Join("\n", lines);
            HasNotice = lines.Count > 0;
        }

        // --- watching ---------------------------------------------------------------------------------------

        /// <summary>
        /// Scripts\ for twllink --install and hand edits, Config\ for the overlay rewriting scripts.cfg. Only while
        /// the tab is on screen; moved when the game folder setting changes.
        /// </summary>
        private void Watch(string engine)
        {
            if (!_active || string.Equals(engine, _watchedEngine, StringComparison.OrdinalIgnoreCase) && _watchers.Count > 0)
                return;

            StopWatching();
            _watchedEngine = engine;
            if (engine == null)
                return;

            AddWatcher(PluginInstallation.ScriptsPath(engine), "*");
            AddWatcher(Path.GetDirectoryName(ScriptsConfig.PathFor(engine)), ScriptsConfig.FileName);
        }

        private void AddWatcher(string folder, string filter)
        {
            if (!Directory.Exists(folder))
                return;

            try
            {
                var watcher = new FileSystemWatcher(folder, filter)
                {
                    NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.Size
                };
                FileSystemEventHandler changed = (_, _) => Dispatcher.UIThread.Post(RestartRescan);
                watcher.Changed += changed;
                watcher.Created += changed;
                watcher.Deleted += changed;
                watcher.Renamed += (_, _) => Dispatcher.UIThread.Post(RestartRescan);
                watcher.EnableRaisingEvents = true;
                _watchers.Add(watcher);
            }
            catch (Exception exception) when (exception is IOException || exception is ArgumentException || exception is UnauthorizedAccessException)
            {
                Logger.Log("Scripts", "cannot watch " + folder + ": " + exception.Message);
            }
        }

        private void RestartRescan()
        {
            if (!_active)
                return;
            _rescan.Stop();
            _rescan.Start();
        }

        private void StopWatching()
        {
            foreach (var watcher in _watchers)
                watcher.Dispose();
            _watchers.Clear();
            _watchedEngine = null;
        }

        // --- actions ----------------------------------------------------------------------------------------

        /// <summary>The row's switch moved. Returns whether the change stands.</summary>
        private bool TrySetEnabled(ScriptRowViewModel row, bool enabled)
        {
            var engine = PluginService.EngineDirectory;
            if (engine == null || row.IsMissing)
                return false;

            // The game is connected: the plugin switches it and writes scripts.cfg. The switch shows the new position
            // now and the row says it is waiting; the bridge's answer or its timeout settles it (ApplyLive).
            if (ScriptsOverlayBridge.IsLive)
            {
                if (ScriptsOverlayBridge.RequestSet(row.FileName, enabled))
                    return true;

                ApplicationNotificationManager.Manager.ShowError("Scripts",
                    "Could not reach the plugin in the game. Switch the script in the game's overlay (Scripts tab).");
                return false;
            }

            // Checked again at the moment of the click: the poll can be up to two seconds behind the game.
            UpdateGameRunning();
            if (GameRunning)
            {
                ApplicationNotificationManager.Manager.ShowInformation("Scripts",
                    "Audiosurf is running and Tweaker is not connected to its plugin - switch the script in the game's overlay (Scripts tab).");
                return false;
            }

            try
            {
                var config = ScriptsConfig.Load(engine);
                config.SetEnabled(row.FileName, enabled);
                config.Save();
            }
            catch (Exception exception) when (exception is IOException || exception is UnauthorizedAccessException)
            {
                ApplicationNotificationManager.Manager.ShowError("Scripts", "Could not write scripts.cfg: " + exception.Message);
                return false;
            }

            var on = Scripts.Count(r => (r == row ? enabled : r.Enabled) && !r.IsMissing);
            Summary = $"{on} of {Scripts.Count(r => !r.IsMissing)} enabled";
            return true;
        }

        /// <summary>Puts one bundled script back (<paramref name="row"/>), or all of them (null), after asking.</summary>
        private async Task RestoreAsync(ScriptRowViewModel row)
        {
            var engine = PluginService.EngineDirectory;
            if (engine == null)
                return;

            var what = row == null ? "the bundled scripts" : "the bundled " + row.Title;
            var message = row == null
                ? "Every bundled script is put back the way this version of Tweaker ships it: bundled scripts you deleted come back, "
                  + "and the ones you edited are replaced - your versions are kept next to them as .old copies."
                : row.IsMissing
                    ? $"{row.FileName} is put back the way this version of Tweaker ships it."
                    : $"{row.FileName} is put back the way this version of Tweaker ships it. Your version is kept next to it as a .old copy.";
            if (ScriptsOverlayBridge.IsLive)
                message += "\n\nAudiosurf is running and connected: the restored scripts are reloaded in the game right away.";
            else if (GameProcessService.IsRunning)
                message += "\n\nAudiosurf is running: it picks the restored files up when you reload the script in the overlay's Scripts tab, or at the next start.";

            if (!await ApplicationNotificationManager.Manager.AskForAction("Restore " + what, message))
                return;

            var names = row == null ? null : new[] { row.FileName };
            var report = await Task.Run(() => PluginInstallation.RestoreScripts(engine, PluginService.PayloadDirectory, names));

            Logger.Log("Scripts",
                $"restore: +{report.Added.Count} ~{report.Updated.Count} backed-up {report.UpdatedWithBackup.Count} errors {report.Errors.Count}");

            // The game: a rescan for the files that came back, then a reload for the ones replaced under a running
            // script (script-manager.md §5). A reload of a file it has not seen rescans too, but the rescan also answers with
            // the whole list - which is what this tab wants after a restore anyway.
            if (report.ChangedAnything && ScriptsOverlayBridge.IsLive)
            {
                ScriptsOverlayBridge.Rescan();
                // .lua only: a bundle's .lua.map is replaced with it, and is not a script the plugin knows by name.
                ScriptsOverlayBridge.Reload(report.Updated
                    .Concat(report.UpdatedWithBackup.Select(b => b.RelativePath))
                    .Where(path => path.EndsWith(".lua", StringComparison.OrdinalIgnoreCase))
                    .Select(Path.GetFileName));
            }

            if (!report.Success)
            {
                var details = string.Join("\n", report.Errors.Select(e => e.RelativePath == null ? e.Message : e.RelativePath + ": " + e.Message));
                ApplicationNotificationManager.Manager.ShowError("Scripts", "Some scripts could not be restored:\n" + details);
            }
            else if (report.UpdatedWithBackup.Count > 0)
            {
                var saved = string.Join("\n", report.UpdatedWithBackup.Select(b => Path.GetFileName(b.RelativePath) + " -> " + b.BackupFileName));
                ApplicationNotificationManager.Manager.ShowInformation("Scripts restored", "Your versions were kept next to them:\n" + saved);
            }
            else if (report.ChangedAnything)
            {
                ApplicationNotificationManager.Manager.ShowSuccess("Scripts", "Bundled scripts restored.");
            }

            await RefreshAsync();
        }
    }
}
