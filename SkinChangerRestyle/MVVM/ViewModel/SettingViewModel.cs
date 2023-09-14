using ASCommander;
using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.MVVM.Model;
using System;
using System.IO;
using System.Windows.Forms;
using SkinChangerRestyle.Core.Utils;


namespace SkinChangerRestyle.MVVM.ViewModel
{
    public enum SettingsFields
    {
        TexturesPath,
        AddSkinsPath
    }

    internal class SettingViewModel : ObservableObject, IDisposable
    {
        public SettingViewModel()
        {
            ScrollAllowed = true;
            SetConfigurationValue = new RelayCommand(AskAndSetConfigValue);
            SelectTempFile = new RelayCommand(SelectTempFileInternal);
            DuplicateTempFile = new RelayCommand(DuplicateTempFileInternal);
            OpenOverlayHelpWindow = new RelayCommand(OpenOverlayHelpPage);
            TexturesFolderPath = SettingsProvider.GameTexturesPath;
            AdditionalSkinsFolderPath = SettingsProvider.SkinsFolderPath;
            IsHotReload = SettingsProvider.HotReload;
            IsShouldCheckTextures = SettingsProvider.ControlSystemActive;
            WatcherTempFile = SettingsProvider.WatcherTempFile;
            _isWatcherEnabled = SettingsProvider.WatcherEnabled;

            IsUWPNotificationsAllowed = SettingsProvider.IsUWPNotificationsAllowed;
            IsUWPNotificationSilent = SettingsProvider.IsUWPNotificationSilent;
            _overlayEnabled = SettingsProvider.IsOverlayEnabled;

            if (_isWatcherEnabled)
            {
                Watcher = TexturesWatcher.Instance;
                Watcher.TargetPath = SettingsProvider.GameTexturesPath;

                IsShouldStoreTextures = SettingsProvider.WatcherShouldStoreTextures;
                IsTempFileOverrided = SettingsProvider.WatcherTempFileOverrided;

                if (IsShouldStoreTextures)
                {
                    Watcher.InitializeTempFile(IsTempFileOverrided ? WatcherTempFile : SettingsProvider.WatcherDefaultTemp);
                }

                Watcher.DiskOperationCompleted += (s, e) =>
                {
                    IsGuiActive = true;
                    OnPropertyChanged(nameof(IsWatcherActive));
                };

                Watcher.Triggered += async (s, e) =>
                {
                    AudiosurfHandle.Instance.Command("ascommand reloadtextures");
                    if (IsShouldStoreTextures)
                    {
                        await Watcher.OverwriteTempFile();
                    }
                };
            }

            AudiosurfHandle.Instance.MessageResieved += OnMessageRecieved;
        }

        public bool IsFastPreview
        {
            get => SettingsProvider.UseFastPreview;
            set
            {
                var isRestart = ApplicationNotificationManager.Manager.AskForAction("Module Parameter Changed",
                    "Turning this parameter needs to restart applicaton. Would you like to continue?");
                if (isRestart)
                {
                    SettingsProvider.UseFastPreview = value;
                    ApplySettings();
                    Utils.Cmd($"taskkill /f /im \"{AppDomain.CurrentDomain.FriendlyName}\" && timeout /t 1 && {AppDomain.CurrentDomain.FriendlyName}");
                }
            }
        }

        public string TexturesFolderPath
        {
            get => _texturesPath;
            set
            {
                _texturesPath = value;
                SettingsProvider.GameTexturesPath = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }

        public string AdditionalSkinsFolderPath
        {
            get => _additionalSkinsPath;
            set
            {
                _additionalSkinsPath = value;
                SettingsProvider.SkinsFolderPath = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }

        public bool IsShouldCheckTextures
        {
            get => _isShouldCheckTextures;
            set
            {
                _isShouldCheckTextures = value;
                SettingsProvider.ControlSystemActive = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }

        public bool IsHotReload
        {
            get => _isHotReload;
            set
            {
                _isHotReload = value;
                SettingsProvider.HotReload = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }
        
        public bool IsShouldUseSafetyInstallation
        {
            get => _isSafeInstall;
            set
            {
                _isSafeInstall = value;
                SettingsProvider.SafeInstall = value;
                ApplySettings();
                OnPropertyChanged();
            }
        }

        public bool IsWatcherActive
        {
            get => Watcher != null && IsGuiActive;
            set
            {
                var isRestart = ApplicationNotificationManager.Manager.AskForAction("Module Parameter Changed",
                    "Turning this parameter needs to restart applicaton. Would you like to continue?");
                if (isRestart)
                {
                    _isWatcherEnabled = value;
                    SettingsProvider.WatcherEnabled = value;
                    ApplySettings();
                    Utils.Cmd($"taskkill /f /im \"{AppDomain.CurrentDomain.FriendlyName}\" && timeout /t 1 && {AppDomain.CurrentDomain.FriendlyName}");
                }
            }
        }

        public string WatcherTempFile
        {
            get => IsTempFileOverrided ? _watcherTempFile : SettingsProvider.WatcherDefaultTemp;
            set
            {
                _watcherTempFile = value;

                if (!File.Exists(_watcherTempFile))
                {
                    if (!Directory.Exists(Path.GetDirectoryName(_watcherTempFile)))
                        Directory.CreateDirectory(Path.GetDirectoryName(_watcherTempFile) ?? throw new InvalidOperationException());

                    using (var _ = File.Create(_watcherTempFile)) { }
                }

                SettingsProvider.WatcherTempFile = value;
                ApplySettings();
            }
        }

        public bool IsShouldStoreTextures
        {
            get => _isShouldStoreTempTextures;
            set
            {
                if (!_isWatcherEnabled) return;
                _isShouldStoreTempTextures = value;
                OnPropertyChanged();
                SettingsProvider.WatcherShouldStoreTextures = value;
                ApplySettings();
                if (value)
                {
                    IsGuiActive = false;
                    OnPropertyChanged(nameof(IsWatcherActive));
                    if (!string.IsNullOrEmpty(Watcher.TempFilePath))
                        Watcher.OverwriteTempFile();
                    else
                        Watcher.InitializeTempFile(WatcherTempFile);
                }
            }
        }

        public bool IsTempFileOverrided
        {
            get => _isWatcherEnabled && _isTempFileOverrided;
            set
            {
                IsGuiActive = false;
                _isTempFileOverrided = value;
                OnPropertyChanged();
                OnPropertyChanged(nameof(IsWatcherActive));
                SettingsProvider.WatcherTempFileOverrided = value;
                Watcher.InitializeTempFile(value ? WatcherTempFile : SettingsProvider.WatcherDefaultTemp);
                ApplySettings();
            }
        }

        public bool IsGuiActive
        {
            get => _isGuiActive; 
            set 
            { 
                _isGuiActive = value;
                OnPropertyChanged();
            }
        }

        public bool IsUWPNotificationsAllowed
        {
            get => _uwpNotificationAllwed;
            set
            {
                _uwpNotificationAllwed = value;
                OnPropertyChanged();
                SettingsProvider.IsUWPNotificationsAllowed = value;
                var notifyMessage = value
                    ? "Windows notification enabled"
                    : "Windows notification disabled. That was the last time";

                if (value) 
                    ApplicationNotificationManager.Manager.ShowUWPNotification("UWP Notification settings", notifyMessage);
                ApplySettings();
            }
        }

        public bool IsUWPNotificationSilent
        {
            get => _uwpNotificationSilent;
            set
            {
                _uwpNotificationSilent = value;
                OnPropertyChanged();
                SettingsProvider.IsUWPNotificationSilent = value;
                ApplySettings();
            }
        }

        public bool IsOverlayEnabled
        {
            get => _overlayEnabled;
            set
            {
                var isRestart = ApplicationNotificationManager.Manager.AskForAction("Module Parameter Changed",
                    "Turning this parameter needs to restart applicaton. Would you like to continue?");
                if (isRestart)
                {
                    _overlayEnabled = value;
                    SettingsProvider.IsOverlayEnabled = value;
                    ApplySettings();
                    Utils.Cmd($"taskkill /f /im \"{AppDomain.CurrentDomain.FriendlyName}\" && timeout /t 1 && {AppDomain.CurrentDomain.FriendlyName}");
                }
            }
        }

        public RelayCommand SetConfigurationValue { get; set; }
        public RelayCommand SelectTempFile { get; set; }
        public RelayCommand DuplicateTempFile { get; set; }
        public RelayCommand OpenOverlayHelpWindow { get; set; }
        public TexturesWatcher Watcher { get; private set; }

        private string _texturesPath;
        private string _additionalSkinsPath;
        private string _watcherTempFile;
        private bool _isWatcherEnabled;
        private bool _isShouldCheckTextures;
        private bool _isShouldStoreTempTextures;
        private bool _isHotReload;
        private bool _isSafeInstall;
        private bool _isTempFileOverrided;
        private bool _isGuiActive;
        private bool _uwpNotificationAllwed;
        private bool _uwpNotificationSilent;
        private bool _overlayEnabled;

        private void AskAndSetConfigValue(object parameter)
        {
            var field = (SettingsFields)parameter;

            var pathSelectionDialog = new FolderBrowserDialog();
            if (pathSelectionDialog.ShowDialog() == DialogResult.OK)
            {
                switch (field)
                {
                    case SettingsFields.TexturesPath:
                        TexturesFolderPath = pathSelectionDialog.SelectedPath;
                        SettingsProvider.GameTexturesPath = TexturesFolderPath;
                        break;
                    case SettingsFields.AddSkinsPath:
                        AdditionalSkinsFolderPath = pathSelectionDialog.SelectedPath;
                        SettingsProvider.SkinsFolderPath = AdditionalSkinsFolderPath;
                        break;
                }
                ApplySettings();
            }
        }

        private void ApplySettings()
        {
            ConfigurationManager.RewriteSettings();
        }

        private void SelectTempFileInternal(object o)
        {
            var saveFileDialog = new SaveFileDialog();
            saveFileDialog.Title = "Select new temp file";
            saveFileDialog.InitialDirectory = Path.GetDirectoryName(Path.GetFullPath(WatcherTempFile));

            if (saveFileDialog.ShowDialog() == DialogResult.OK)
            {
                WatcherTempFile = saveFileDialog.FileName;
                OnPropertyChanged(nameof(WatcherTempFile));
                Watcher.InitializeTempFile(WatcherTempFile);
            }
        }

        private void DuplicateTempFileInternal(object o)
        {
            var saveFileDialog = new SaveFileDialog();
            saveFileDialog.Title = "Select duplicate output";
            saveFileDialog.InitialDirectory = Path.GetDirectoryName(Path.GetFullPath(WatcherTempFile));

            if (saveFileDialog.ShowDialog() == DialogResult.OK)
            {
                File.Copy(WatcherTempFile, saveFileDialog.FileName);
            }
        }

        private void OnMessageRecieved(object sender, string messageContent)
        {
            if (messageContent.Contains("tw-Apply-configuration"))
            {
                var config = messageContent.Substring("tw-Apply-configuration".Length).Trim().Split(new[] { "; " }, StringSplitOptions.RemoveEmptyEntries);
                foreach (var cfg in config)
                {
                    var keyValuePair = cfg.Split(':');
                    if (keyValuePair.Length != 2) continue;
                    ConfigurationManager.UpdateSection(keyValuePair[0], keyValuePair[1]);
                }
            }
        }

        private void OpenOverlayHelpPage(object _)
        {
            GuidePageHelper.ShowOverlayHelp();
        }

        public void Dispose()
        {
            Watcher.Dispose();
        }
    }
}
