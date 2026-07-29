using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using TweakerUI.Core;

namespace TweakerUI.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            // Overrides the XAML-declared Title so the caption SingleInstanceGuard searches for can
            // only ever come from one place (see AppShell.MainWindowTitle); the XAML keeps its own
            // literal purely so the designer preview still shows a caption.
            Title = AppShell.MainWindowTitle;
            Opened += OnOpened;
        }

        private void OnOpened(object sender, System.EventArgs e)
        {
            ApplicationNotificationManager.Manager.Attach(this);
        }

        private void OnTitleBarPointerPressed(object sender, PointerPressedEventArgs e)
        {
            if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
                BeginMoveDrag(e);
        }

        private void OnMinimizeClick(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void OnMaximizeClick(object sender, RoutedEventArgs e)
        {
            WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;
        }

        private void OnCloseClick(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void OnResizeGripPointerPressed(object sender, PointerPressedEventArgs e)
        {
            if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
                BeginResizeDrag(WindowEdge.SouthEast, e);
        }
    }
}
