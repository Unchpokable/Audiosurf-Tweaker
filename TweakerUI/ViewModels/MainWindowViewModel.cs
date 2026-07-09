using System;
using Avalonia.Media;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace TweakerUI.ViewModels
{
    public partial class MainWindowViewModel : ViewModelBase
    {
        public MainWindowViewModel()
        {
            SkinChangerVM = new SkinChangerViewModel();
            ColorsVM = new ColorsConfiguratorViewModel();
            ServerSwapperVM = new ServerSwapperViewModel();
            TweakerVM = new TweakerViewModel();
            SettingsVM = new SettingViewModel();

            currentView = SkinChangerVM;

            _asHandle = AudiosurfHandle.Instance;
            _asHandle.StateChanged += OnAudiosurfStateChanged;
        }

        public SkinChangerViewModel SkinChangerVM { get; }
        public ColorsConfiguratorViewModel ColorsVM { get; }
        public ServerSwapperViewModel ServerSwapperVM { get; }
        public TweakerViewModel TweakerVM { get; }
        public SettingViewModel SettingsVM { get; }

        [ObservableProperty]
        private ViewModelBase currentView;

        public string AudiosurfStatusMessage => _asHandle.StateMessage;

        public IBrush AudiosurfStatusBrush => Brush.Parse(_asHandle.StateColor ?? "#ff0000");

        private readonly AudiosurfHandle _asHandle;

        [RelayCommand]
        private void ShowSkinChanger() => CurrentView = SkinChangerVM;

        [RelayCommand]
        private void ShowColors() => CurrentView = ColorsVM;

        [RelayCommand]
        private void ShowServerSwapper() => CurrentView = ServerSwapperVM;

        [RelayCommand]
        private void ShowTweaker() => CurrentView = TweakerVM;

        [RelayCommand]
        private void ShowSettings() => CurrentView = SettingsVM;

        [RelayCommand]
        private void ResetBridge() => _asHandle.ReinitializeWndProcMessageService();

        [RelayCommand]
        private void EnableAutoHandling() => _asHandle.StartAutoHandling();

        private void OnAudiosurfStateChanged(object sender, EventArgs e)
        {
            OnPropertyChanged(nameof(AudiosurfStatusMessage));
            OnPropertyChanged(nameof(AudiosurfStatusBrush));
        }
    }
}
