using System;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using AudiosurfInterface;
using AudiosurfInterface.Bridge;
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
        public App()
        {
            AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
            SkinPackager.OperationFailed += OnSkinPackagerOperationFailed;
            LegacyConverter.ConversionFailed += OnLegacyConversionFailed;
            Logger.ReadWriteException += OnLoggerReadWriteException;
        }

        public override void Initialize()
        {
            AvaloniaXamlLoader.Load(this);
        }

        public override void OnFrameworkInitializationCompleted()
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                // MainWindowViewModel's constructor loads config (ConfigurationManager.InitializeEnvironment)
                // as its first act, so SettingsProvider.IsDarkTheme is only known once it returns - applying
                // the theme has to happen after this line, before the window is actually shown, or the user
                // briefly sees the XAML-default Light variant flash before flipping to their saved choice.
                var mainViewModel = new MainWindowViewModel();
                AudiosurfHandle.Instance.Diagnostic += OnAudiosurfBridgeDiagnostic;
                AudiosurfHandle.Instance.CommunicationFailed += OnAudiosurfCommunicationFailed;
                AudiosurfHandle.Instance.ServiceSuspended += OnAudiosurfServiceSuspended;
                ThemeService.Apply(SettingsProvider.IsDarkTheme);

                desktop.MainWindow = new MainWindow
                {
                    DataContext = mainViewModel,
                };

                // Exit rather than ShutdownRequested: the latter can still be cancelled, and tearing
                // playback down under a shutdown the user then called off would leave the Quick Player
                // tab alive but gutted.
                desktop.Exit += (_, _) => mainViewModel.Dispose();
            }

            base.OnFrameworkInitializationCompleted();
        }

        private void OnSkinPackagerOperationFailed(string context, Exception exception)
        {
            Logger.Log("SkinPackager", $"{context}: {exception}");
        }

        private void OnLegacyConversionFailed(string path, Exception exception)
        {
            Logger.Log("LegacyConverter", $"Failed to convert '{path}': {exception}");
        }

        private void OnAudiosurfBridgeDiagnostic(AsBridgeDiagnostic diagnostic)
        {
            Logger.Log($"AudiosurfBridge/{diagnostic.Context}", diagnostic.Exception != null
                ? $"{diagnostic.Message}\n{diagnostic.Exception}"
                : diagnostic.Message);
        }

        // AudiosurfHandle's registration watchdog gave up after quickstart/plain/bridge-restart all
        // went unanswered by the game - already logged as an Error-level Diagnostic, this is just the
        // user-facing half of the same event (see AudiosurfHandle.GiveUpRegistrationLocked).
        private void OnAudiosurfCommunicationFailed(string message)
        {
            Dispatcher.UIThread.Post(() => ApplicationNotificationManager.Manager.ShowError(
                "Audiosurf connection broken", message));
        }

        // The interface stopped itself on purpose - currently only the overlay inject guard does this,
        // on finding a stale plugin already sitting in the game process (see OverlayHelper). Raised
        // from a background thread, hence the same Dispatcher.Post as above.
        private void OnAudiosurfServiceSuspended(string reason)
        {
            Logger.Log("AudiosurfHandle", $"Service suspended: {reason}");
            Dispatcher.UIThread.Post(() => ApplicationNotificationManager.Manager.ShowError(
                "Audiosurf service stopped", reason));
        }

        // The logger itself couldn't write to disk (restricted MyDocuments path, log dir deleted while
        // running, etc.) - surfacing this beats losing every subsequent Log() call silently, since
        // Logger is usually the only place an original failure's details are recorded at all.
        private void OnLoggerReadWriteException(Exception exception)
        {
            ApplicationNotificationManager.Manager.ShowWarning("Logging error",
                $"Could not write to the log file: {exception.Message}");
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
            Logger.Log("Unhandled exception", formattedMessage);

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