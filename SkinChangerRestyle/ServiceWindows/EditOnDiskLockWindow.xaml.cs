using System;
using System.Windows;
using System.Windows.Forms;
using System.Windows.Input;

namespace SkinChangerRestyle
{
    /// <summary>
    /// Логика взаимодействия для EditOnDiskLockWindow.xaml
    /// </summary>
    public partial class EditOnDiskLockWindow : Window
    {
        private Timer _timer;

        public EditOnDiskLockWindow()
        {
            InitializeComponent();
            CompleteButton.IsEnabled = false;
            _timer = new Timer();
            _timer.Interval = 5000;
            _timer.Tick += UnlockCompleteButton;
            _timer.Start();
        }

        private void UnlockCompleteButton(object sender, EventArgs e)
        {
            CompleteButton.IsEnabled = true;
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void Label_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.LeftButton == MouseButtonState.Pressed)
                DragMove();
        }
    }
}
