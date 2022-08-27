using SkinChangerRestyle.Core.Extensions;
using SkinChangerRestyle.MVVM.ViewModel;
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
    /// Логика взаимодействия для ProcessSelectionWindow.xaml
    /// </summary>
    public partial class ProcessSelectionWindow : Window
    {
        public ProcessSelectionWindow()
        {
            InitializeComponent();
            if (DataContext != null)
                ((ProcessSelectionViewModel)DataContext).CloseWindow = new Core.RelayCommand(o => Close());
        }

        private void Button_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }

        private void Border_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.LeftButton == MouseButtonState.Pressed)
                DragMove();
        }

        protected override void OnClosed(EventArgs e)
        {
            base.OnClosed(e);
            var disposableDataContext = DataContext as IDisposable;
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }
    }
}
