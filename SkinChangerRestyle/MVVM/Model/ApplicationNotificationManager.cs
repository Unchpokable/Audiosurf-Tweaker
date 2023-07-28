using Notification.Wpf;
using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.Extensions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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
                Extensions.ShowUWPNotification(title, message);
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
    }
}
