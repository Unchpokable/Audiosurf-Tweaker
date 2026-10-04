using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerCore.Scripting;

namespace TweakerUI.Models
{
    /// <summary>
    /// One script in the Scripts tab: a <see cref="GameScript"/> dressed for display, plus the switch. The row
    /// decides nothing itself - whether a script may be switched right now, and what switching means, is the
    /// tab's (<see cref="ViewModels.ScriptsViewModel"/>), which hands in the two callbacks below.
    /// </summary>
    public partial class ScriptRowViewModel : ObservableObject
    {
        private readonly Func<ScriptRowViewModel, bool, bool> _toggle;
        private readonly Func<ScriptRowViewModel, Task> _restore;
        private GameScript _script;
        private bool _enabled;

        // What the running plugin says (script-manager-roadmap.md §9, С5b) and a switch still waiting for it to
        // confirm. Both null without a link; then the file - scripts.cfg - is all there is.
        private ScriptLiveState _live;
        private bool? _pending;

        public ScriptRowViewModel(GameScript script, Func<ScriptRowViewModel, bool, bool> toggle, Func<ScriptRowViewModel, Task> restore)
        {
            _toggle = toggle;
            _restore = restore;
            Update(script);
        }

        public GameScript Script => _script;

        /// <summary>The identity the list is keyed by: scripts.cfg and the plugin know a script by its file name.</summary>
        public string FileName => _script.FileName;

        public string Title => _script.Header.Name;

        /// <summary>"puzzlepro_hud.lua · v1.4.0 · by Unchpokable" - what is known, in that order.</summary>
        public string Subtitle
        {
            get
            {
                var parts = new List<string> { _script.FileName };
                if (_script.Header.Version.Length > 0)
                    parts.Add("v" + _script.Header.Version);
                if (_script.Header.Author.Length > 0)
                    parts.Add("by " + _script.Header.Author);
                return string.Join("  ·  ", parts);
            }
        }

        public string Description => _script.Header.Description;

        public bool HasDescription => _script.Header.Description.Length > 0;

        public bool IsMissing => _script.IsMissing;

        public bool IsBundled => _script.Origin == ScriptOrigin.Shipped && !_script.IsMissing;

        public bool IsEdited => _script.Origin == ScriptOrigin.Modified;

        public bool IsUsers => _script.Origin == ScriptOrigin.User;

        /// <summary>Built by twllink from a project: its errors in the overlay point into the project's files, when the map is there.</summary>
        public bool IsProjectBuild => _script.Header.IsBundle;

        public string ProjectBuildHint => _script.HasMap
            ? "Built by twllink from a project. The in-game Scripts tab shows its errors in the project's own files."
            : "Built by twllink from a project, but its .lua.map is missing - errors are shown as lines of the built file.";

        public bool CanRestore => _script.Restorable;

        public string RestoreHint => _script.IsMissing
            ? "Put this bundled script back"
            : "Put back the bundled version - yours is kept as a .old copy next to it";

        public bool HasBackups => _script.Backups.Count > 0;

        public string BackupsText => _script.Backups.Count == 1 ? "1 saved copy" : _script.Backups.Count + " saved copies";

        public string BackupsHint =>
            "Your versions of this script, kept when a bundled update or a restore replaced it:\n" + string.Join("\n", _script.Backups);

        /// <summary>Whether the tab lets the switch move at all right now; set by the tab, not by the script.</summary>
        [ObservableProperty]
        private bool canToggle;

        public bool Enabled
        {
            get => _enabled;
            set
            {
                if (value == _enabled)
                    return;

                if (_toggle(this, value))
                {
                    _enabled = value;
                    OnPropertyChanged();
                    return;
                }

                // Refused: the switch already shows the new position, and a PropertyChanged raised from inside
                // the binding's own write is swallowed - it has to arrive after it for the switch to go back.
                Dispatcher.UIThread.Post(() => OnPropertyChanged(nameof(Enabled)));
            }
        }

        /// <summary>A switch is waiting for the game to confirm it.</summary>
        public bool IsPending => _pending.HasValue;

        /// <summary>
        /// The line under the description while the game is connected and has something to say: why the script
        /// failed or was suspended, that the game has not seen the file yet, or that a switch is waiting. Empty when
        /// all is well - a running script needs no caption.
        /// </summary>
        public string LiveText
        {
            get
            {
                if (_pending.HasValue)
                    return "Waiting for the game...";
                if (_live == null)
                    return "";

                switch (_live.Status)
                {
                    case ScriptLiveStatus.Failed:
                        return _live.Message.Length > 0 ? "Failed: " + _live.Message : "Failed to load";
                    case ScriptLiveStatus.Suspended:
                        return "Suspended: " + (_live.Message.Length > 0 ? _live.Message : "it kept failing or took too long");
                    case ScriptLiveStatus.Missing:
                        return _script.IsMissing ? "" : "The game has not picked this file up yet - refresh to have it look again.";
                    default:
                        return "";
                }
            }
        }

        public bool HasLiveText => LiveText.Length > 0;

        public bool LiveFailed => !_pending.HasValue && _live?.Status == ScriptLiveStatus.Failed;

        public bool LiveSuspended => !_pending.HasValue && _live?.Status == ScriptLiveStatus.Suspended;

        /// <summary>Takes a fresh scan of the same script, keeping the row - and so the list's scroll position - in place.</summary>
        public void Update(GameScript script)
        {
            _script = script;
            _enabled = Shown();
            OnPropertyChanged(string.Empty);
        }

        /// <summary>
        /// What the game says about this script, and the switch position waiting for its word. Raises only what
        /// moved: this runs for every row on every state the plugin sends.
        /// </summary>
        public void ApplyLive(ScriptLiveState live, bool? pending)
        {
            if (Equals(live, _live) && pending == _pending)
                return;

            _live = live;
            _pending = pending;

            var shown = Shown();
            if (shown != _enabled)
            {
                _enabled = shown;
                OnPropertyChanged(nameof(Enabled));
            }

            OnPropertyChanged(nameof(IsPending));
            OnPropertyChanged(nameof(LiveText));
            OnPropertyChanged(nameof(HasLiveText));
            OnPropertyChanged(nameof(LiveFailed));
            OnPropertyChanged(nameof(LiveSuspended));
        }

        /// <summary>The switch shows the click being confirmed, else the game's word, else scripts.cfg.</summary>
        private bool Shown()
        {
            if (_pending.HasValue)
                return _pending.Value;
            if (_live != null && _live.Status != ScriptLiveStatus.Missing)
                return _live.Enabled;
            return _script.Enabled;
        }

        [RelayCommand]
        private Task Restore() => _restore(this);

        [RelayCommand]
        private void ShowInFolder()
        {
            if (_script.IsMissing)
                return;

            try
            {
                // Arguments, not ArgumentList: Explorer wants the quotes around the path only, "/select," outside them.
                using (Process.Start(new ProcessStartInfo("explorer.exe", "/select,\"" + _script.Path + "\"")))
                {
                }
            }
            catch (Exception exception) when (exception is System.ComponentModel.Win32Exception || exception is InvalidOperationException)
            {
                Core.Logger.Log("Scripts", "cannot open Explorer: " + exception.Message);
            }
        }
    }
}
