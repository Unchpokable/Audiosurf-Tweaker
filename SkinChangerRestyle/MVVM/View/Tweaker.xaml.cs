using System.Windows.Controls;

namespace SkinChangerRestyle.MVVM.View
{
    /// <summary>
    /// Логика взаимодействия для Tweaker.xaml
    /// </summary>
    public partial class Tweaker : UserControl
    {
        public Tweaker()
        {
            InitializeComponent();
        }

        private void TextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            ConsoleOutput.ScrollToEnd();
        }
    }
}
