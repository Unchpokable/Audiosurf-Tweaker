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
    /// Логика взаимодействия для ImageViewWindow.xaml
    /// </summary>
    public partial class ImageViewWindow : Window
    {
        public int ProportialHeight => ((int)Width * 9) / 16;

        public ImageViewWindow(ImageSource image)
        {
            Width = 800;
            Height = 450;
            InitializeComponent();
            ImageViewport.Source = image;
        }

        protected override void OnDeactivated(EventArgs e)
        {
            Close();
            base.OnDeactivated(e);
        }

        private void Border_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ButtonState == MouseButtonState.Pressed)
                DragMove();
        }
    }
}
