using SkinChangerRestyle.Core;
using System.Windows.Media;

namespace SkinChangerRestyle.MVVM.Model
{
    internal class InteractableScreenshot
    {
        public ImageSource Image { get; set; }
        public RelayCommand ShowBigPicture { get; set; }

        public InteractableScreenshot(ImageSource image)
        {
            Image = image;
            ShowBigPicture = new RelayCommand(o =>
            {
                var imagePreviewWindow = new ImageViewWindow(Image);
                imagePreviewWindow.Show();
            });
        }
    }
}
