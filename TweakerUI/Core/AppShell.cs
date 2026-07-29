using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;

namespace TweakerUI.Core
{
    // Small shared lookup for "the app's single main window" - the same ambient access the old WPF
    // code got for free via Application.Current.MainWindow, needed by anything that has to own a
    // dialog or resolve a TopLevel (notifications, file pickers) without threading a Window reference
    // through every view model constructor.
    internal static class AppShell
    {
        // Single source of truth for the main window's caption: MainWindow's constructor assigns it
        // over whatever the XAML declared, and SingleInstanceGuard looks the running instance up by
        // it. Two literals would silently drift apart and break the focus handover, not the build.
        public const string MainWindowTitle = "Audiosurf Tweaker";

        public static Window MainWindow =>
            Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop
                ? desktop.MainWindow
                : null;
    }
}
