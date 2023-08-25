using SkinChangerRestyle.Core;
using System.Diagnostics;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;

namespace SkinChangerRestyle
{

    /// <summary>
    /// Логика взаимодействия для MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private Logger _logger;

        public static Dispatcher WindowDispatcher => Application.Current.Dispatcher;

        public MainWindow()
        {
            //AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
            InitializeComponent();
            Focus();
            _logger = new Logger();
        }

        private void Window_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ButtonState == MouseButtonState.Pressed)
                DragMove();
        }

        private void MinimizeButton_Click(object sender, RoutedEventArgs e)
        {
            if (Application.Current.MainWindow.WindowState == WindowState.Normal || Application.Current.MainWindow.WindowState == WindowState.Maximized)
            {
                Application.Current.MainWindow.WindowState = WindowState.Minimized;
            }
        }

        private void MaximizeButton_Click(object sender, RoutedEventArgs e)
        {
            Application.Current.MainWindow.WindowState = Application.Current.MainWindow.WindowState == WindowState.Normal ? WindowState.Maximized : WindowState.Normal;
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Application.Current.Shutdown();
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
           Process.Start(SettingsProvider.GameTexturesPath ?? @"C:/");
        }
#if !DEBUG
        //private void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
        //{
        //    var exception = e.ExceptionObject as Exception;
        //    var formattedMessage = $"Ooops! An unhandled exception occurred!\n{exception.Message}\nStack Trace: {exception.StackTrace}\n";
        //    MessageBox.Show(formattedMessage, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
        //    _logger.Log("Initialization fault", formattedMessage);
        //}

        //private void OnFirstChanceUnhandledException(object sender, FirstChanceExceptionEventArgs e)
        //{
        //    var exception = e.Exception;
        //    var formattedMessage = $"Ooops! An unhandled exception occurred!\n{exception.Message}\nStack Trace: {exception.StackTrace}\n";
        //    MessageBox.Show(formattedMessage, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
        //}
#endif
    }
}
