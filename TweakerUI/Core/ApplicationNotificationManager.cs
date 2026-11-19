using System;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Controls.Notifications;
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
            _host?.Show(new Notification(title, message, type));
        }

        public void ShowError(string title, string message) => Show(title, message, NotificationType.Error);
        public void ShowSuccess(string title, string message) => Show(title, message, NotificationType.Success);
        public void ShowWarning(string title, string message) => Show(title, message, NotificationType.Warning);
        public void ShowInformation(string title, string message) => Show(title, message, NotificationType.Information);

        // Awaited, not blocking: Avalonia's Window.ShowDialog<T>() runs on Dispatcher.UIThread rather
        // than a nested message loop, so a synchronous wrapper would deadlock the UI thread on itself.
        public Task<bool> AskForAction(string title, string message)
        {
            return AskForActionAsync(title, message);
        }

        private static async Task<bool> AskForActionAsync(string title, string message)
        {
            var result = await TweakerDialogWindow.ShowAsync(AppShell.MainWindow, message, title, TweakerDialogButtons.OKCancel);
            return result == TweakerDialogResult.OK;
        }

        /// <summary>
        /// The same dialog with an arbitrary button list, returning the index of the one pressed. The plugin
        /// flows need three outcomes each (plugin-offline-mode.md В§6.4, В§6.5), and folding a third state into
        /// <see cref="AskForAction"/>'s bool is how "cancel" and "no" end up indistinguishable.
        /// </summary>
        public Task<int> AskForChoice(string title, string message, params string[] buttons)
        {
            return TweakerDialogWindow.ShowChoiceAsync(AppShell.MainWindow, message, title, TweakerDialogChoice.Of(buttons));
        }

        public Task ShowImportantInfo(string title, string message)
        {
            return TweakerDialogWindow.ShowAsync(AppShell.MainWindow, message, title, TweakerDialogButtons.OK);
        }
    }
}
