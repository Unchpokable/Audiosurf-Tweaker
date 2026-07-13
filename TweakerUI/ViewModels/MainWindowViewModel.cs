using System;
using System.Threading.Tasks;
using Avalonia.Media;
using Avalonia.Threading;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerUI.Core;

namespace TweakerUI.ViewModels
{
    public partial class MainWindowViewModel : ViewModelBase
    {
        public MainWindowViewModel()
        {
            // Mirrors the donor MainViewModel's constructor order exactly: config has to be loaded into
            // SettingsProvider before any child VM is built, since several of them (SkinChangerViewModel
            // among them) read SettingsProvider.* values straight from their own constructors. This was
            // never actually wired up in TweakerUI before now - every SettingsProvider field has been
            // sitting at its C# default (false/null) through every phase-4 smoke test to date.
            ConfigurationManager.InitializationFaultCallback += OnConfigurationFault;
            ConfigurationManager.SetUpDefaultSettings();
            ConfigurationManager.InitializeEnvironment();

            SkinChangerVM = new SkinChangerViewModel();
            ColorsVM = new ColorsConfiguratorViewModel();
            TweakerVM = new TweakerViewModel();
            SettingsVM = new SettingViewModel();

            currentView = SkinChangerVM;

            _asHandle = AudiosurfHandle.Instance;
            _asHandle.StateChanged += OnAudiosurfStateChanged;
        }

        public SkinChangerViewModel SkinChangerVM { get; }
        public ColorsConfiguratorViewModel ColorsVM { get; }
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
        private void ShowTweaker() => CurrentView = TweakerVM;

        [RelayCommand]
        private void ShowSettings() => CurrentView = SettingsVM;

        // ReinitializeWndProcMessageService disposes the old bridge connection synchronously (up to a
        // 2s Thread.Join plus a blocking Process.Kill) - running it inline on the UI thread froze the
        // whole window for that long every time the user hit Reset.
        [RelayCommand]
        private Task ResetBridge() => Task.Run(() => _asHandle.ReinitializeWndProcMessageService());

        private void OnAudiosurfStateChanged(object sender, EventArgs e)
        {
            OnPropertyChanged(nameof(AudiosurfStatusMessage));
            OnPropertyChanged(nameof(AudiosurfStatusBrush));
        }

        // Fires from ConfigurationManager's own constructor-time call above, when the window/its
        // TopLevel-attached notification host don't exist yet (AppShell.MainWindow is still null at
        // this point in startup) - DispatcherPriority.Loaded defers it past that window, the same
        // priority already relied on elsewhere in this codebase for "after the window is actually up"
        // timing (see SkinChangerView's rename-focus handling).
        private static void OnConfigurationFault(Exception exception)
        {
            Dispatcher.UIThread.Post(() => ApplicationNotificationManager.Manager.ShowErrorWnd(
                "Settings initialization error",
                "Could not detect the Audiosurf installation. Check the Settings tab, set the game textures path manually, then restart Audiosurf Tweaker."),
                DispatcherPriority.Loaded);
        }
    }
}
