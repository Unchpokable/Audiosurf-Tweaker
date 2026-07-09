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
        public static Window MainWindow =>
            Application.Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop
                ? desktop.MainWindow
                : null;
    }
}
