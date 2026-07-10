using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media.Imaging;

namespace TweakerUI.Views
{
    public partial class ImageViewWindow : Window
    {
        // Parameterless constructor required by the Avalonia XAML loader/previewer.
        public ImageViewWindow() : this(null)
        {
        }

        public ImageViewWindow(Bitmap image)
        {
            InitializeComponent();
            ImageViewport.Source = image;
            Deactivated += (_, _) => Close();
        }

        private void OnPointerPressed(object sender, PointerPressedEventArgs e)
        {
            if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
                BeginMoveDrag(e);
        }
    }
}
