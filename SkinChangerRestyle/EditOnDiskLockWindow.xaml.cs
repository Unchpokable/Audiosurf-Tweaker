using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
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
        public EditOnDiskLockWindow()
        {
            InitializeComponent();
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
