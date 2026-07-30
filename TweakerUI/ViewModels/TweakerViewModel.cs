using System;
using Avalonia.Threading;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerUI.Core;
using TweakerUI.Core.Utils;
using TweakerUI.Models;

namespace TweakerUI.ViewModels
{
    public partial class TweakerViewModel : ViewModelBase
    {
        public TweakerViewModel()
        {
            _audiosurfHandle = AudiosurfHandle.Instance;
            IsAudiosurfConnected = _audiosurfHandle.IsValid;
            _audiosurfHandle.StateChanged += (_, _) => IsAudiosurfConnected = _audiosurfHandle.IsValid;

            _console = new TweakerConsole();
            _console.ContentUpdated += (_, _) => OnPropertyChanged(nameof(ConsoleContent));

            OverlayHelper.TweakRequested += OnOverlayTweakRequested;

            // The switches below show the *global* value; a Quick Player entry can be sitting on top of
            // one for the duration of a track (GameConfigState's override stack), in which case the
            // game is not currently doing what this tab displays. Rather than moving the switches under
            // Quick Player's feet, each row grows a "QP" badge - the in-game overlay marks the same
            // state the same way (Docs/Internal/overlay-protocol.md, "Временные твики Quick Player").
            foreach (var snapshot in GameConfigState.Manager.GetKnownTweakSnapshots())
                ApplyOverrideBadge(snapshot);
            GameConfigState.Manager.StateChanged += OnGameConfigStateChanged;
        }

        private void OnGameConfigStateChanged(object sender, GameConfigStateChangedEventArgs e)
        {
            // Raised from whichever thread changed the config - the report dispatch thread for a Quick
            // Player track boundary, a thread-pool thread for its start. These are bound properties.
            Dispatcher.UIThread.Post(() => ApplyOverrideBadge(e.Snapshot));
        }

        private void ApplyOverrideBadge(GameConfigSnapshot snapshot)
        {
            var definition = GameTweakCatalog.FindByConfigKey(snapshot.Key);
            if (definition == null)
                return;

            var overridden = snapshot.OverrideSource == GameConfigOverrideSource.QuickPlayer;
            switch (definition.WireName)
            {
                case "InvisibleRoad": InvisibleRoadOverriddenByQuickPlayer = overridden; break;
                case "HiddenSongTitle": HiddenSongOverriddenByQuickPlayer = overridden; break;
                case "SidewinderCamera": SidewinderCameraOverriddenByQuickPlayer = overridden; break;
                case "BankingCamera": BankingCameraOverriddenByQuickPlayer = overridden; break;
                case "FreerideNoBlocks": FreerideNoBlocksOverriddenByQuickPlayer = overridden; break;
                case "FreerideBlocksCaterpillars": FreerideBlocksCaterpillarsOverriddenByQuickPlayer = overridden; break;
                case "FreerideAutoAdvanceDisable": FreerideAutoAdvanceDisableOverriddenByQuickPlayer = overridden; break;
            }
        }

        // Reverse-sync (Docs/Internal/overlay-protocol.md, NOTIFY_TWEAK): the user flipped a tweak
        // directly in the in-game overlay. The property is updated without its normal global write,
        // then SetAndClearOverrides commits the requested effective value and removes any QP source.
        // OverlayHelper observes that GameConfigState transition and sends the confirming TWEAK_SET.
        // An unrecognized wire name is silently ignored, like an unknown TWEAK_SET in the plugin.
        private void OnOverlayTweakRequested(object sender, TweakRequestedEventArgs e)
        {
            var definition = GameTweakCatalog.FindByWireName(e.WireName);
            if (definition == null)
                return;

            // The overlay toggle displays the effective value. Its click therefore commits that
            // newly displayed value immediately and cancels a Quick Player override rather than
            // editing a hidden global baseline underneath it.
            _suppressGlobalApply = true;
            try
            {
                switch (e.WireName)
                {
                    case "InvisibleRoad": InvisibleRoadTweakActive = e.Enabled; break;
                    case "HiddenSongTitle": HiddenSongTweakActive = e.Enabled; break;
                    case "SidewinderCamera": SidewinderCameraTweakActive = e.Enabled; break;
                    case "BankingCamera": BankingCameraTweakActive = e.Enabled; break;
                    case "FreerideNoBlocks": FreerideNoBlocksTweakActive = e.Enabled; break;
                    case "FreerideBlocksCaterpillars": FreerideBlocksCaterpillarsTweakActive = e.Enabled; break;
                    case "FreerideAutoAdvanceDisable": FreerideAutoAdvanceDisableTweakActive = e.Enabled; break;
                }
            }
            finally
            {
                _suppressGlobalApply = false;
            }

            GameConfigState.Manager.SetAndClearOverrides(
                definition.ConfigKey,
                definition.ToConfigValue(e.Enabled));
        }

        [ObservableProperty]
        private bool isAudiosurfConnected;

        /// <summary>
        /// Tooltip shared by every "QP" badge - one string rather than seven copies in the axaml, and
        /// it has to spell out both halves of GameConfigState.UpdateGlobal's rule, because "what
        /// happens when I click this" genuinely differs depending on which way the switch is going.
        /// </summary>
        public string QuickPlayerOverrideHint =>
            "Quick Player is overriding this tweak for the current track, so the game may not match "
            + "the switch right now. The switch sets your global value: set it to what Quick Player is "
            + "already forcing and that becomes permanent immediately, set it the other way and it "
            + "takes effect when the track ends.";

        [ObservableProperty]
        private bool invisibleRoadOverriddenByQuickPlayer;

        [ObservableProperty]
        private bool hiddenSongOverriddenByQuickPlayer;

        [ObservableProperty]
        private bool sidewinderCameraOverriddenByQuickPlayer;

        [ObservableProperty]
        private bool bankingCameraOverriddenByQuickPlayer;

        [ObservableProperty]
        private bool freerideNoBlocksOverriddenByQuickPlayer;

        [ObservableProperty]
        private bool freerideBlocksCaterpillarsOverriddenByQuickPlayer;

        [ObservableProperty]
        private bool freerideAutoAdvanceDisableOverriddenByQuickPlayer;

        [ObservableProperty]
        private bool invisibleRoadTweakActive;

        partial void OnInvisibleRoadTweakActiveChanged(bool value)
        {
            ApplyGlobalTweak("InvisibleRoad", value);
        }

        [ObservableProperty]
        private bool hiddenSongTweakActive;

        partial void OnHiddenSongTweakActiveChanged(bool value)
        {
            ApplyGlobalTweak("HiddenSongTitle", value);
        }

        [ObservableProperty]
        private bool sidewinderCameraTweakActive;

        partial void OnSidewinderCameraTweakActiveChanged(bool value)
        {
            ApplyGlobalTweak("SidewinderCamera", value);
        }

        [ObservableProperty]
        private bool bankingCameraTweakActive;

        partial void OnBankingCameraTweakActiveChanged(bool value)
        {
            ApplyGlobalTweak("BankingCamera", value);
        }

        [ObservableProperty]
        private bool freerideNoBlocksTweakActive;

        partial void OnFreerideNoBlocksTweakActiveChanged(bool value)
        {
            ApplyGlobalTweak("FreerideNoBlocks", value);
        }

        [ObservableProperty]
        private bool freerideBlocksCaterpillarsTweakActive;

        partial void OnFreerideBlocksCaterpillarsTweakActiveChanged(bool value)
        {
            ApplyGlobalTweak("FreerideBlocksCaterpillars", value);
        }

        [ObservableProperty]
        private bool freerideAutoAdvanceDisableTweakActive;

        partial void OnFreerideAutoAdvanceDisableTweakActiveChanged(bool value)
        {
            ApplyGlobalTweak("FreerideAutoAdvanceDisable", value);
        }

        public string ConsoleContent => _console.ToString();

        [RelayCommand]
        private void Send(string param)
        {
            if (string.Equals(param, GameProtocol.CloseAudiosurf, StringComparison.OrdinalIgnoreCase))
            {
                KillAudiosurf();
                return;
            }
            _audiosurfHandle.Command(GameProtocol.Command(param));
        }

        [RelayCommand]
        private void FlushConsole()
        {
            _console.Flush();
            OnPropertyChanged(nameof(ConsoleContent));
        }

        private readonly AudiosurfHandle _audiosurfHandle;
        private readonly TweakerConsole _console;
        private bool _suppressGlobalApply;

        private void ApplyGlobalTweak(string wireName, bool enabled)
        {
            if (_suppressGlobalApply)
                return;

            var definition = GameTweakCatalog.FindByWireName(wireName);
            if (definition != null)
                GameConfigState.Manager.Set(definition.ConfigKey, definition.ToConfigValue(enabled));
        }

        private void KillAudiosurf() => Utils.Cmd($"taskkill /f /pid {_audiosurfHandle.GamePID}");
    }
}
