using System;
using System.IO;
using System.Windows;

namespace SkinChangerRestyle
{
    /// <summary>
    /// Логика взаимодействия для GuidancePage.xaml
    /// </summary>
    public partial class GuidancePage : Window
    {
        public GuidancePage(string localPath)
        {
            InitializeComponent();
            var absolutePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, localPath);

            var absoluteUri = new Uri(absolutePath, UriKind.Absolute);
            //ContentViewBrowser.Navigate(absoluteUri);
            ContentViewBrowser.Source = absoluteUri;
        }
    }
}
