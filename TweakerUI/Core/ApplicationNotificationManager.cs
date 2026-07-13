using System;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Controls.Notifications;
using Microsoft.Toolkit.Uwp.Notifications;
using TweakerUI.Core.Dialogs;
using TweakerUI.Views.Dialogs;

namespace TweakerUI.Core
{
    // Replaces WPF's Notification.Wpf.NotificationManager. Avalonia's WindowNotificationManager only
    // overlays within an attached TopLevel (no separate "float over the taskbar while minimized" mode
    // like Notification.Wpf had), so the old ShowOverWindow/ShowOverTaskbar split collapses into one
    // path here - acceptable since the app is a single borderless window anyway.
    internal class ApplicationNotificationManager
    {
        private ApplicationNotificationManager() { }

        public static ApplicationNotificationManager Manager => _instance ??= new ApplicationNotificationManager();

        private static ApplicationNotificationManager _instance;
        private WindowNotificationManager _host;

        // Called once from the main window once it's constructed (application shell stage) - every
        // Show* call before that silently no-ops rather than throwing, same tolerance the old code had
        // toward notifications firing before RegisterContext ran.
        public void Attach(TopLevel topLevel)
        {
            _host = new WindowNotificationManager(topLevel) { MaxItems = 3 };
        }

        public void Show(string title, string message, NotificationType type)
        {
            if (SettingsProvider.IsUWPNotificationsAllowed)
                ShowUWPNotification(title, message);
            else
                _host?.Show(new Notification(title, message, type));
        }

        public void ShowError(string title, string message) => Show(title, message, NotificationType.Error);
        public void ShowSuccess(string title, string message) => Show(title, message, NotificationType.Success);
        public void ShowWarning(string title, string message) => Show(title, message, NotificationType.Warning);
        public void ShowInformation(string title, string message) => Show(title, message, NotificationType.Information);

        public void ShowErrorWnd(string title, string message) => Show(title, message, NotificationType.Error);
        public void ShowSuccessWnd(string title, string message) => Show(title, message, NotificationType.Success);
        public void ShowWarningWnd(string title, string message) => Show(title, message, NotificationType.Warning);
        public void ShowInformationWnd(string title, string message) => Show(title, message, NotificationType.Information);

        // Replaces the old blocking WPF ShowDialog() - Avalonia's Window.ShowDialog<T>() runs on
        // Dispatcher.UIThread rather than a nested message loop, so a synchronous wrapper here would
        // deadlock the UI thread waiting on itself. Call sites become "await"-ed as each module is ported.
        public Task<bool> AskForAction(string title, string message)
        {
            return AskForActionCore(title, message);
        }

        private static async Task<bool> AskForActionCore(string title, string message)
        {
            var result = await TweakerDialogWindow.ShowAsync(AppShell.MainWindow, message, title, TweakerDialogButtons.OKCancel);
            return result == TweakerDialogResult.OK;
        }

        public Task ShowImportantInfo(string title, string message)
        {
            return TweakerDialogWindow.ShowAsync(AppShell.MainWindow, message, title, TweakerDialogButtons.OK);
        }

        public void ShowUWPNotification(string caption, string message)
        {
            var toast = new ToastContentBuilder()
                .AddHeader("0", caption, new ToastArguments())
                .SetToastDuration(ToastDuration.Short)
                .AddText(message);

            if (SettingsProvider.IsUWPNotificationSilent)
                toast.AddAudio(new ToastAudio() { Silent = true });

            try
            {
                toast.Show();
            }
            catch
            {
                // Toast notifications commonly throw for unpackaged Win32 apps without a registered
                // AUMID/shortcut (e.g. a portable build) - falling back to the in-app notification host
                // beats losing the message entirely.
                _host?.Show(new Notification(caption, message, NotificationType.Information));
            }
        }
    }
}
