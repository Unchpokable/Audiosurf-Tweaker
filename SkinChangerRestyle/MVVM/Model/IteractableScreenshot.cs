using SkinChangerRestyle.Core;
using SkinChangerRestyle.MVVM.ViewModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
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
