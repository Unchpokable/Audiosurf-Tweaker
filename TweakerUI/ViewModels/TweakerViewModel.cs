using System;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
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
        }

        [ObservableProperty]
        private bool isAudiosurfConnected;

        [ObservableProperty]
        private bool invisibleRoadTweakActive;

        partial void OnInvisibleRoadTweakActiveChanged(bool value) =>
            GameConfigState.Manager.Set(GameProtocol.RoadVisible, !value);

        [ObservableProperty]
        private bool hiddenSongTweakActive;

        partial void OnHiddenSongTweakActiveChanged(bool value) =>
            GameConfigState.Manager.Set(GameProtocol.ShowSongName, !value);

        [ObservableProperty]
        private bool sidewinderCameraTweakActive;

        partial void OnSidewinderCameraTweakActiveChanged(bool value) =>
            GameConfigState.Manager.Set(GameProtocol.Sidewinder, value);

        [ObservableProperty]
        private bool bankingCameraTweakActive;

        partial void OnBankingCameraTweakActiveChanged(bool value) =>
            GameConfigState.Manager.Set(GameProtocol.UseBankingCamera, value);

        [ObservableProperty]
        private bool freerideNoBlocksTweakActive;

        partial void OnFreerideNoBlocksTweakActiveChanged(bool value) =>
            GameConfigState.Manager.Set(GameProtocol.FreerideBlocks, !value);

        [ObservableProperty]
        private bool freerideBlocksCaterpillarsTweakActive;

        partial void OnFreerideBlocksCaterpillarsTweakActiveChanged(bool value) =>
            GameConfigState.Manager.Set(GameProtocol.FreerideCaterpillars, value);

        [ObservableProperty]
        private bool freerideAutoAdvanceDisableTweakActive;

        partial void OnFreerideAutoAdvanceDisableTweakActiveChanged(bool value) =>
            GameConfigState.Manager.Set(GameProtocol.FreerideAutoAdvance, !value);

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
