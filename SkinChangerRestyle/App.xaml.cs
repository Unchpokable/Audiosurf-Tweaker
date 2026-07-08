using System;
using System.Windows;
using System.Windows.Threading;
using TweakerCore.Engine;
using SkinChangerRestyle.Core;

namespace SkinChangerRestyle
{
    /// <summary>
    /// Логика взаимодействия для App.xaml
    /// </summary>
    public partial class App : Application
    {
        private readonly Logger _logger = new Logger();

        public App()
        {
            AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
            DispatcherUnhandledException += OnDispatcherUnhandledException;
            SkinPackager.OperationFailed += OnSkinPackagerOperationFailed;
        }

        private void OnDispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
        {
            LogAndNotify("UI thread exception", e.Exception);
            e.Handled = true;
        }

        private void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            if (e.ExceptionObject is Exception exception)
                LogAndNotify("Unhandled exception", exception);
        }

        private void OnSkinPackagerOperationFailed(string context, Exception exception)
        {
            _logger.Log("SkinPackager", $"{context}: {exception}");
        }

        private void LogAndNotify(string title, Exception exception)
        {
            var formattedMessage = $"{exception.Message}\nStack Trace: {exception.StackTrace}";
            _logger.Log(title, formattedMessage);
            MessageBox.Show($"Ooops! An unhandled exception occurred!\n{formattedMessage}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }
}
