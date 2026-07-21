using System;
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

            OverlayHelper.OverlayReady += (_, _) => PushOverlaySnapshot();
        }

        // Wire-name -> current value, one shot on OverlayHelper.OverlayReady (see PushOverlaySnapshot).
        // Same seven tweaks/names as Docs/Internal/overlay-protocol.md's wire-name table.
        private void PushOverlaySnapshot()
        {
            OverlayHelper.SetTweak("InvisibleRoad", InvisibleRoadTweakActive);
            OverlayHelper.SetTweak("HiddenSongTitle", HiddenSongTweakActive);
            OverlayHelper.SetTweak("SidewinderCamera", SidewinderCameraTweakActive);
            OverlayHelper.SetTweak("BankingCamera", BankingCameraTweakActive);
            OverlayHelper.SetTweak("FreerideNoBlocks", FreerideNoBlocksTweakActive);
            OverlayHelper.SetTweak("FreerideBlocksCaterpillars", FreerideBlocksCaterpillarsTweakActive);
            OverlayHelper.SetTweak("FreerideAutoAdvanceDisable", FreerideAutoAdvanceDisableTweakActive);
        }

        [ObservableProperty]
        private bool isAudiosurfConnected;

        [ObservableProperty]
        private bool invisibleRoadTweakActive;

        partial void OnInvisibleRoadTweakActiveChanged(bool value)
        {
            GameConfigState.Manager.Set(GameProtocol.RoadVisible, !value);
            OverlayHelper.SetTweak("InvisibleRoad", value);
        }

        [ObservableProperty]
        private bool hiddenSongTweakActive;

        partial void OnHiddenSongTweakActiveChanged(bool value)
        {
            GameConfigState.Manager.Set(GameProtocol.ShowSongName, !value);
            OverlayHelper.SetTweak("HiddenSongTitle", value);
        }

        [ObservableProperty]
        private bool sidewinderCameraTweakActive;

        partial void OnSidewinderCameraTweakActiveChanged(bool value)
        {
            GameConfigState.Manager.Set(GameProtocol.Sidewinder, value);
            OverlayHelper.SetTweak("SidewinderCamera", value);
        }

        [ObservableProperty]
        private bool bankingCameraTweakActive;

        partial void OnBankingCameraTweakActiveChanged(bool value)
        {
            GameConfigState.Manager.Set(GameProtocol.UseBankingCamera, value);
            OverlayHelper.SetTweak("BankingCamera", value);
        }

        [ObservableProperty]
        private bool freerideNoBlocksTweakActive;

        partial void OnFreerideNoBlocksTweakActiveChanged(bool value)
        {
            GameConfigState.Manager.Set(GameProtocol.FreerideBlocks, !value);
            OverlayHelper.SetTweak("FreerideNoBlocks", value);
        }

        [ObservableProperty]
        private bool freerideBlocksCaterpillarsTweakActive;

        partial void OnFreerideBlocksCaterpillarsTweakActiveChanged(bool value)
        {
            GameConfigState.Manager.Set(GameProtocol.FreerideCaterpillars, value);
            OverlayHelper.SetTweak("FreerideBlocksCaterpillars", value);
        }

        [ObservableProperty]
        private bool freerideAutoAdvanceDisableTweakActive;

        partial void OnFreerideAutoAdvanceDisableTweakActiveChanged(bool value)
        {
            GameConfigState.Manager.Set(GameProtocol.FreerideAutoAdvance, !value);
            OverlayHelper.SetTweak("FreerideAutoAdvanceDisable", value);
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

        private void KillAudiosurf() => Utils.Cmd($"taskkill /f /pid {_audiosurfHandle.GamePID}");
    }
}
