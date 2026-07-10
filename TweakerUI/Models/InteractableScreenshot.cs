using Avalonia.Media.Imaging;
using CommunityToolkit.Mvvm.Input;
using TweakerUI.Views;

namespace TweakerUI.Models
{
    // Double-click enlarge, ported from the old WPF ImageViewWindow ahead of the rest of the
    // ServiceWindows (stage 4.5) - the user flagged its absence as a regression, not a deferred nicety.
    public class InteractableScreenshot
    {
        public Bitmap Image { get; }
        public IRelayCommand ShowBigPictureCommand { get; }

        public InteractableScreenshot(Bitmap image)
        {
            Image = image;
            ShowBigPictureCommand = new RelayCommand(() => new ImageViewWindow(Image).Show());
        }
    }
}
