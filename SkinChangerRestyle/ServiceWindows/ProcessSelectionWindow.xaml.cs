using SkinChangerRestyle.MVVM.ViewModel;
using System;
using System.Windows;
using System.Windows.Input;

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
