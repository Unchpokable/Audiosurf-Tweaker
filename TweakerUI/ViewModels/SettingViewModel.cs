using System;
using System.IO;
using System.Threading.Tasks;
using AudiosurfInterface;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using TweakerUI.Core;
using TweakerUI.Models;
using TweakerUI.Services;

namespace TweakerUI.ViewModels
{
    // Ported from SkinChangerRestyle.MVVM.ViewModel.SettingViewModel. Biggest behavioral change:
    // in the donor, "Textures Watcher - Enabled" (and "Fast Preview"/"Overlay", both since dropped
    // from this app) needed a full app restart to take effect - purely because the watcher was only
    // ever constructed once, in the constructor, gated on the persisted flag. Here IsWatcherActive
    // starts/stops TexturesWatcher.Instance live, so nothing in this tab needs a restart any more.
    public partial class SettingViewModel : ViewModelBase
    {
        public SettingViewModel()
        {
            TexturesFolderPath = SettingsProvider.GameTexturesPath;
            AdditionalSkinsFolderPath = SettingsProvider.SkinsFolderPath;
            IsHotReload = SettingsProvider.HotReload;
            IsShouldCheckTextures = SettingsProvider.ControlSystemActive;
            IsShouldUseSafetyInstallation = SettingsProvider.SafeInstall;
            IsFastPreview = SettingsProvider.UseFastPreview;
            IsUWPNotificationsAllowed = SettingsProvider.IsUWPNotificationsAllowed;
            IsUWPNotificationSilent = SettingsProvider.IsUWPNotificationSilent;
            // Assigned directly, not through the property setter - the theme is already applied by the
            // time this VM is constructed (App.axaml.cs applies it before the window is shown), so
            // there's no need to call ThemeService.Apply a second time for the value it started at.
            isDarkTheme = SettingsProvider.IsDarkTheme;

            // Assigned directly (not through the property setters below) so StartWatcher can read the
            // persisted temp-file preferences without re-triggering their side effects twice.
            watcherTempFile = SettingsProvider.WatcherTempFile;
            isTempFileOverrided = SettingsProvider.WatcherTempFileOverrided;
            isShouldStoreTextures = SettingsProvider.WatcherShouldStoreTextures;

            if (SettingsProvider.WatcherEnabled)
            {
                isWatcherActive = true;
                StartWatcher();
            }
        }

        [ObservableProperty]
        private bool isDarkTheme;

        partial void OnIsDarkThemeChanged(bool value)
        {
            SettingsProvider.IsDarkTheme = value;
            ApplySettings();
            ThemeService.Apply(value);
        }

        [ObservableProperty]
        private string texturesFolderPath;

        partial void OnTexturesFolderPathChanged(string value)
        {
            SettingsProvider.GameTexturesPath = value;
            ApplySettings();
            if (Watcher != null && Directory.Exists(value))
                Watcher.TargetPath = value;
        }

        [ObservableProperty]
        private string additionalSkinsFolderPath;

        partial void OnAdditionalSkinsFolderPathChanged(string value)
        {
            SettingsProvider.SkinsFolderPath = value;
            ApplySettings();
        }

        [ObservableProperty]
        private bool isHotReload;

        partial void OnIsHotReloadChanged(bool value)
        {
            SettingsProvider.HotReload = value;
            ApplySettings();
        }

        [ObservableProperty]
        private bool isShouldCheckTextures;

        partial void OnIsShouldCheckTexturesChanged(bool value)
        {
            SettingsProvider.ControlSystemActive = value;
            ApplySettings();
        }

        [ObservableProperty]
        private bool isShouldUseSafetyInstallation;

        partial void OnIsShouldUseSafetyInstallationChanged(bool value)
        {
            SettingsProvider.SafeInstall = value;
            ApplySettings();
        }

        // Trades memory for speed: eagerly decodes a skin card's screenshots when its data loads
        // instead of on selection. Off by default - decode-on-selection (with skeleton placeholders
        // while it loads) is the tested/default path; this only changes behavior for skins loaded or
        // reloaded after the toggle flips, no restart needed.
        [ObservableProperty]
        private bool isFastPreview;

        partial void OnIsFastPreviewChanged(bool value)
        {
            SettingsProvider.UseFastPreview = value;
            ApplySettings();
        }

        [ObservableProperty]
        private bool isUWPNotificationsAllowed;

        partial void OnIsUWPNotificationsAllowedChanged(bool value)
        {
            SettingsProvider.IsUWPNotificationsAllowed = value;
            if (value)
                ApplicationNotificationManager.Manager.ShowUWPNotification("UWP Notification settings", "Windows notifications enabled");
            ApplySettings();
        }

        [ObservableProperty]
        private bool isUWPNotificationSilent;

        partial void OnIsUWPNotificationSilentChanged(bool value)
        {
            SettingsProvider.IsUWPNotificationSilent = value;
            ApplySettings();
        }

        public bool IsWatcherActive
        {
            get => isWatcherActive;
            set
            {
                if (!SetProperty(ref isWatcherActive, value)) return;
                SettingsProvider.WatcherEnabled = value;
                ApplySettings();
                if (value)
                    StartWatcher();
                else
                    StopWatcher();
            }
        }
        private bool isWatcherActive;

        public bool IsShouldStoreTextures
        {
            get => isShouldStoreTextures;
            set
            {
                if (!IsWatcherActive || !SetProperty(ref isShouldStoreTextures, value)) return;
                SettingsProvider.WatcherShouldStoreTextures = value;
                ApplySettings();
                if (value)
                    _ = InitializeTempFileAsync();
            }
        }
        private bool isShouldStoreTextures;

        public bool IsTempFileOverrided
        {
            get => isTempFileOverrided;
            set
            {
                if (!IsWatcherActive || !SetProperty(ref isTempFileOverrided, value)) return;
                SettingsProvider.WatcherTempFileOverrided = value;
                OnPropertyChanged(nameof(WatcherTempFile));
                ApplySettings();
                _ = InitializeTempFileAsync();
            }
        }
        private bool isTempFileOverrided;

        public string WatcherTempFile
        {
            get => IsTempFileOverrided ? watcherTempFile : SettingsProvider.WatcherDefaultTemp;
            private set
            {
                watcherTempFile = value;
                SettingsProvider.WatcherTempFile = value;
                OnPropertyChanged();
                ApplySettings();
            }
        }
        private string watcherTempFile;

        internal TexturesWatcher Watcher { get; private set; }

        [RelayCommand]
        private async Task BrowseTexturesPath()
        {
            var path = await FileDialogService.OpenFolderAsync("Select the Audiosurf textures folder");
            if (path != null)
                TexturesFolderPath = path;
        }

        [RelayCommand]
        private async Task BrowseSkinsPath()
        {
            var path = await FileDialogService.OpenFolderAsync("Select an additional skins folder");
            if (path != null)
                AdditionalSkinsFolderPath = path;
        }

        [RelayCommand]
        private async Task SelectTempFile()
        {
            var path = await FileDialogService.SaveFileAsync("Select temp file location", "temp.tasp");
            if (path == null)
                return;

            WatcherTempFile = path;
            _ = InitializeTempFileAsync();
        }

        [RelayCommand]
        private async Task DuplicateTempFile()
        {
            if (!File.Exists(WatcherTempFile))
            {
                ApplicationNotificationManager.Manager.ShowError("Duplicate temp file", "The current temp file does not exist yet - nothing to duplicate");
                return;
            }

            var path = await FileDialogService.SaveFileAsync("Select duplicate output", "temp.tasp");
            if (path == null)
                return;

            try
            {
                File.Copy(WatcherTempFile, path, overwrite: true);
            }
            catch (Exception ex)
            {
                ApplicationNotificationManager.Manager.ShowError("Duplicate temp file", $"Could not copy the temp file: {ex.Message}");
            }
        }

        private void StartWatcher()
        {
            if (string.IsNullOrEmpty(SettingsProvider.GameTexturesPath) || !Directory.Exists(SettingsProvider.GameTexturesPath))
            {
                ApplicationNotificationManager.Manager.ShowWarning("Textures Watcher", "Game textures path is not set or does not exist - the watcher was not started. Set a valid path and re-enable it");
                // The public setter already persisted "enabled" before calling in here - undo that
                // since the watcher never actually started, so the toggle and the saved config agree.
                isWatcherActive = false;
                SettingsProvider.WatcherEnabled = false;
                ApplySettings();
                OnPropertyChanged(nameof(IsWatcherActive));
                return;
            }

            Watcher = TexturesWatcher.Instance;
            Watcher.TargetPath = SettingsProvider.GameTexturesPath;
            Watcher.Triggered += OnWatcherTriggered;

            if (IsShouldStoreTextures)
                _ = InitializeTempFileAsync();
        }

        private void StopWatcher()
        {
            if (Watcher == null)
                return;

            Watcher.Triggered -= OnWatcherTriggered;
            TexturesWatcher.Reset();
            Watcher = null;
        }

        private Task InitializeTempFileAsync() => Watcher?.InitializeTempFileAsync(WatcherTempFile) ?? Task.CompletedTask;

        private async void OnWatcherTriggered(object sender, FileSystemEventArgs e)
        {
            AudiosurfHandle.Instance.Command(GameProtocol.Command(GameProtocol.ReloadTextures));
            if (IsShouldStoreTextures)
                await (Watcher?.OverwriteTempFile() ?? Task.CompletedTask);
        }

        private static void ApplySettings() => ConfigurationManager.RewriteSettings();
    }
}
