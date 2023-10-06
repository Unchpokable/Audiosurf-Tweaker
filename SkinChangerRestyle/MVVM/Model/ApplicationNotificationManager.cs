using Notification.Wpf;
using SkinChangerRestyle.Core;
using System;
using System.Windows;
using Microsoft.Toolkit.Uwp.Notifications;
using SkinChangerRestyle.Core.Dialogs;

namespace SkinChangerRestyle.MVVM.Model
{
    internal class ApplicationNotificationManager
    {
        private ApplicationNotificationManager()
        {
            _notificationManager = new NotificationManager();
        }

        public static ApplicationNotificationManager Manager
        {
            get
            {
                if (_instance == null)
                    _instance = new ApplicationNotificationManager();
                return _instance;
            }
        }

        private static ApplicationNotificationManager _instance;
        private NotificationManager _notificationManager; 
        private object _dataContext;

        public void RegisterContext(object context)
        {
            if (context == null)
                throw new ArgumentNullException(nameof(context));
            _dataContext = context;
        }

        public void ShowOverWindow(string title, string message, NotificationType type, string areaName = "WindowArea", Action onClickAction = null)
        {
            _notificationManager.Show(title, message, type, areaName, onClick: onClickAction);
        }

        public void ShowOverTaskbar(string title, string message, NotificationType type, Action onClickAction = null)
        {
            _notificationManager.Show(title, message, type, onClick: onClickAction);
        }

        public void Show(string title, string message, NotificationType type)
        {
            if (SettingsProvider.IsUWPNotificationsAllowed)
                ShowUWPNotification(title, message);
            else
                ShowOverTaskbar(title, message, type);
        }

        public void ShowError(string title, string message)
            => Show(title, message, NotificationType.Error);

        public void ShowSuccess(string title, string message)
            => Show(title, message, NotificationType.Success);

        public void ShowWarning(string title, string message)
            => Show(title, message, NotificationType.Warning);

        public void ShowInformation(string title, string message)
            => Show(title, message, NotificationType.Information);

        public void ShowErrorWnd(string title, string message)
            => ShowOverWindow(title, message, NotificationType.Error);

        public void ShowSuccessWnd(string title, string message)
            => ShowOverWindow(title, message, NotificationType.Success);

        public void ShowWarningWnd(string title, string message)
            => ShowOverWindow(title, message, NotificationType.Warning);

        public void ShowInformationWnd(string title, string message)
            => ShowOverWindow(title, message, NotificationType.Information);

        public void ShowUWPNotification(string caption, string message)
        {
            var toast = new ToastContentBuilder()
                .AddHeader("0", caption, new ToastArguments())
                .SetToastDuration(ToastDuration.Short)
                .AddText(message);

            if (SettingsProvider.IsUWPNotificationSilent)
                toast.AddAudio(new ToastAudio() { Silent = true });
            toast.Show();
        }

        public bool AskForAction(string title, string message)
        {
            var dialog = new TweakerDialog(message, title, MessageBoxButton.OKCancel, Application.Current.MainWindow);
            dialog.ShowDialog();
            return dialog.Result == TweakerDialogResult.OK;
        }
    }
}
