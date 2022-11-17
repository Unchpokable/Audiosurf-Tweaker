using System.Windows;
using System.Windows.Controls;

namespace SkinChangerRestyle.MVVM.View
{
    /// <summary>
    /// Логика взаимодействия для ColorConfiguratorView.xaml
    /// </summary>
    public partial class ColorConfiguratorView : UserControl
    {
        public ColorConfiguratorView()
        {
            InitializeComponent();
        }

        private void ListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (e.AddedItems.Count == 0) return;

            ((ListBox)sender).ScrollIntoView(e.AddedItems[0]);
        }
    }
}
