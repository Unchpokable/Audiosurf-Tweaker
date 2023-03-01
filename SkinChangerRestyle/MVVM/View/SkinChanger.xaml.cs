using System.Windows.Controls;

namespace SkinChangerRestyle.MVVM.View
{
    /// <summary>
    /// Логика взаимодействия для SkinChanger.xaml
    /// </summary>
    public partial class SkinChanger : UserControl
    {
        public SkinChanger()
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
