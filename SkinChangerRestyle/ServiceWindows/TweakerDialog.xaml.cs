using System.Windows;
using SkinChangerRestyle.Core.Dialogs;

namespace SkinChangerRestyle
{
    /// <summary>
    /// Логика взаимодействия для TweakerDialog.xaml
    /// </summary>
    public partial class TweakerDialog : Window
    {
        public TweakerDialog(string message, string caption, MessageBoxButton buttons)
        {
            InitializeComponent();
            DataContext = this;
            Message = message;
            Caption = caption;
            OkVisibility = buttons == MessageBoxButton.OK || buttons == MessageBoxButton.OKCancel ? Visibility.Visible : Visibility.Collapsed;
            CancelVisibility = buttons == MessageBoxButton.OKCancel ? Visibility.Visible : Visibility.Collapsed;
        }

        public TweakerDialogResult Result { get; private set; }

        public string Message { get; }
        public string Caption { get; }
        public Visibility OkVisibility { get; }
        public Visibility CancelVisibility { get; }

        private void OK_Click(object sender, RoutedEventArgs e)
        {
            Result = TweakerDialogResult.OK;
            Close();
        }

        private void Cancel_Click(object sender, RoutedEventArgs e)
        {
            Result = TweakerDialogResult.Cancel;
            Close();
        }
    }
}
