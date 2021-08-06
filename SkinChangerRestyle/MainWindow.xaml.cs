namespace SkinChangerRestyle
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Text;
    using System.Threading.Tasks;
    using System.Threading;
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Data;
    using System.Windows.Documents;
    using System.Windows.Input;
    using System.Windows.Media;
    using System.Windows.Media.Imaging;
    using System.Windows.Navigation;
    using System.Windows.Shapes;
    using SkinChangerRestyle.MVVM.View;

    /// <summary>
    /// Логика взаимодействия для MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private int currentPage;
        private int pages;

        public MainWindow()
        {
            InitializeComponent();
            currPageLabel.Text = "1";
            currentPage = 1;
            pages = 10; //ONLY FOR TESTS!!!
            maxPagesLabel.Text = pages.ToString();
        }

        private void Window_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ButtonState == MouseButtonState.Pressed)
                DragMove();
        }

        private void loadNextPageButton_Click(object sender, RoutedEventArgs e)
        {
            if (currentPage < pages)
            {
                currentPage++;
                currPageLabel.Text = currentPage.ToString();
            }
        }

        private void loadPrevPageButton_Click(object sender, RoutedEventArgs e)
        {
            if (currentPage > 1)
            {
                currentPage--;
                currPageLabel.Text = currentPage.ToString();
            }
        }
    }
}
