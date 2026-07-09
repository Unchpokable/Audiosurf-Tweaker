using System;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using TweakerCore.Engine;
using TweakerUI.Core;
using TweakerUI.Core.Dialogs;
using TweakerUI.ViewModels;
using TweakerUI.Views;
using TweakerUI.Views.Dialogs;

namespace TweakerUI
{
    public partial class App : Application
    {
        private readonly Logger _logger = new Logger();

        public App()
        {
            AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
            SkinPackager.OperationFailed += OnSkinPackagerOperationFailed;
        }

        public override void Initialize()
        {
            AvaloniaXamlLoader.Load(this);
        }

        public override void OnFrameworkInitializationCompleted()
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                desktop.MainWindow = new MainWindow
                {
                    DataContext = new MainWindowViewModel(),
                };
            }

            base.OnFrameworkInitializationCompleted();
        }

        private void OnSkinPackagerOperationFailed(string context, Exception exception)
        {
            _logger.Log("SkinPackager", $"{context}: {exception}");
        }

        // Avalonia has no WPF-style DispatcherUnhandledException event to catch UI-thread exceptions
        // (e.g. from a bound command) short of wrapping every call site - this only catches exceptions
        // that would otherwise crash the process outright (AppDomain-level), same floor WPF's
        // AppDomain.UnhandledException handler gave.
        private void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            if (e.ExceptionObject is not Exception exception)
                return;

            var formattedMessage = $"{exception.Message}\nStack Trace: {exception.StackTrace}";
            _logger.Log("Unhandled exception", formattedMessage);

            try
            {
                TweakerDialogWindow.ShowAsync(AppShell.MainWindow, $"Ooops! An unhandled exception occurred!\n{formattedMessage}", "Error", TweakerDialogButtons.OK);
            }
            catch
            {
                // The process is already going down for an unrelated reason; a failure to show the
                // error dialog itself shouldn't throw a second exception on top of it.
            }
        }
    }
}