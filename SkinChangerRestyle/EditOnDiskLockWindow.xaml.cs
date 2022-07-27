using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Forms;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

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
    }
}
