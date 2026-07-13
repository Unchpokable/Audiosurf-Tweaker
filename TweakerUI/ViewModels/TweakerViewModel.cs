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
            _audiosurfHandle.Command($"asconfig roadvisible {(!value).ToString().ToLower()}");

        [ObservableProperty]
        private bool hiddenSongTweakActive;

        partial void OnHiddenSongTweakActiveChanged(bool value) =>
            _audiosurfHandle.Command($"asconfig showsongname {(!value).ToString().ToLower()}");

        [ObservableProperty]
        private bool sidewinderCameraTweakActive;

        partial void OnSidewinderCameraTweakActiveChanged(bool value) =>
            _audiosurfHandle.Command($"asconfig sidewinder {value.ToString().ToLower()}");

        [ObservableProperty]
        private bool bankingCameraTweakActive;

        partial void OnBankingCameraTweakActiveChanged(bool value) =>
            _audiosurfHandle.Command($"asconfig usebankingcamera {value.ToString().ToLower()}");

        [ObservableProperty]
        private bool freerideNoBlocksTweakActive;

        partial void OnFreerideNoBlocksTweakActiveChanged(bool value) =>
            _audiosurfHandle.Command($"asconfig freerideblocks {(!value).ToString().ToLower()}");

        [ObservableProperty]
        private bool freerideBlocksCaterpillarsTweakActive;

        partial void OnFreerideBlocksCaterpillarsTweakActiveChanged(bool value) =>
            _audiosurfHandle.Command($"asconfig freeridecaterpillars {value.ToString().ToLower()}");

        [ObservableProperty]
        private bool freerideAutoAdvanceDisableTweakActive;

        partial void OnFreerideAutoAdvanceDisableTweakActiveChanged(bool value) =>
            _audiosurfHandle.Command($"asconfig freerideautoadvance {(!value).ToString().ToLower()}");

        public string ConsoleContent => _console.ToString();

        [RelayCommand]
        private void Send(string param)
        {
            if (string.Equals(param, "closeaudiosurf", StringComparison.OrdinalIgnoreCase))
            {
                KillAudiosurf();
                return;
            }
            _audiosurfHandle.Command($"ascommand {param}");
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
