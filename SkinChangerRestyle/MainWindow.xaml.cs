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
    using SkinChangerRestyle.MVVM.Model;

    /// <summary>
    /// Логика взаимодействия для MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            Thread.Sleep(1000);
            InitializeComponent();
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
           Process.Start(SettingsProvider.GameTexturesPath ?? @"C:/");
        }
    }
}
