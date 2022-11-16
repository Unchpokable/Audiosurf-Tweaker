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

        private void MenuItem_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var menuItemSender = (MenuItem)sender;
                var rootMenu = (ContextMenu)menuItemSender.Parent;
                var rootObject = (Border)rootMenu.PlacementTarget;
                Clipboard.SetText($"#{rootObject.Background.ToString().Substring(3)}");
            }
            catch
            {
                MessageBox.Show("Something went wrong... Unable to copy this color to clipboard", "Oops!", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void ListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (e.AddedItems.Count == 0) return;

            ((ListBox)sender).ScrollIntoView(e.AddedItems[0]);
        }
    }
}
