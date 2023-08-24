using System;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;

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
