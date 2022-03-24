namespace SkinChangerRestyle
{
    using System.Threading;
    using System.Windows;
    using System.Windows.Input;
    using System.Diagnostics;
    using SkinChangerRestyle.Core;
    using ASCommander;
    using ASCommander.PInvoke;
    using System.Drawing;
    using System.Windows.Media;
    using System.Runtime.InteropServices;
    using System;
    using System.Windows.Controls;
    using System.Collections.Generic;

    /// <summary>
    /// Логика взаимодействия для MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private WndProcMessageService wndProcMessageService;
        private bool audiosurfConnected;
        private string asNotConnectedStatusColor = "#9c0202";
        private string asConnectedStatusColor = "#029c07";
        private string asWaitForRegistratingColor = "#9c9c02";
        private Dictionary<string, string> addFeaturesChecksEnableCommandRoute = new Dictionary<string, string>()
        {
            {"hideRoad", "asconfig roadvisible false" },
            {"sidewinder", "asconfig sidewinder true" },
            {"bankcam", "asconfig usebankingcamera true" },
            {"hidesong", "asconfig showsongname false" }
        };

        private Dictionary<string, string> addFeaturesChecksDisableCommandRoute = new Dictionary<string, string>()
        {
            {"hideRoad", "asconfig roadvisible true" },
            {"sidewinder", "asconfig sidewinder false" },
            {"bankcam", "asconfig usebankingcamera false" },
            {"hidesong", "asconfig showsongname true" }
        };

        public MainWindow()
        {
            Thread.Sleep(1000);
            InitializeComponent();
            wndProcMessageService = new WndProcMessageService();
            wndProcMessageService.MessageRecieved += OnMessageRecieved;
            audiosurfConnected = false;
            StatusLabel.Background = (SolidColorBrush)new BrushConverter().ConvertFromString(asNotConnectedStatusColor);
            StatusLabelContent.Text = "Audiosurf Not Connected";
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
            if (Application.Current.MainWindow.WindowState == WindowState.Normal)
            {
                Application.Current.MainWindow.WindowState = WindowState.Maximized;
            }
            else
            {
                Application.Current.MainWindow.WindowState = WindowState.Normal;
            }
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Application.Current.Shutdown();
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
           Process.Start(EnvironmentContainer.gamePath ?? @"C:/");
        }

        private void ShowSubForm(object sender, RoutedEventArgs e)
        {
            SkinInstallationDetails details = new SkinInstallationDetails();
            details.Show();
        }

        private void ConnectAudiosurfWindow(object sender, RoutedEventArgs e)
        {
            var handle = WinAPI.FindWindow(null, "Audiosurf");
            if (handle == IntPtr.Zero)
            {
                System.Windows.MessageBox.Show("Unable to find Audiosurf window", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }
            StatusLabel.Background = (SolidColorBrush)new BrushConverter().ConvertFromString(asWaitForRegistratingColor);
            StatusLabelContent.Text = "Handled. Wait for AS approve";
            wndProcMessageService.Handle(handle);
            wndProcMessageService.Command(WinAPI.WM_COPYDATA, "ascommand registerlistenerwindow AsMsgHandler");
        }

        private void OnMessageRecieved(object sender, System.Windows.Forms.Message message)
        {
            if (message.Msg == WinAPI.WM_COPYDATA)
            {
                var cds = (COPYDATASTRUCT)message.GetLParam(typeof(COPYDATASTRUCT));
                if (cds.cbData > 0)
                {
                    if (cds.lpData.Contains("successfullyregistered"))
                    {
                        StatusLabel.Background = (SolidColorBrush)new BrushConverter().ConvertFromString(asConnectedStatusColor);
                        StatusLabelContent.Text = "Audiosurf connected";
                    }
                }
            }
        }

        private void OnAddFeaturesCheckboxChecked(object sender, RoutedEventArgs e)
        {
            var checkbox = sender as CheckBox;
            wndProcMessageService.Command(WinAPI.WM_COPYDATA, addFeaturesChecksEnableCommandRoute[checkbox.Name]);
        }

        private void OnAddFeaturesCheckboxUnchecked(object sender, RoutedEventArgs e)
        {
            var checkbox = sender as CheckBox;
            wndProcMessageService.Command(WinAPI.WM_COPYDATA, addFeaturesChecksDisableCommandRoute[checkbox.Name]);
        }
    }
}
