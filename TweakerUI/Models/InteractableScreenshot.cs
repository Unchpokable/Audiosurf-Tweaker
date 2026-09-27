using System;
using Avalonia.Media.Imaging;
using CommunityToolkit.Mvvm.Input;
using TweakerUI.Views;

namespace TweakerUI.Models
{
    // Double-click enlarge, ported from the old WPF ImageViewWindow ahead of the rest of the
    // ServiceWindows (stage 4.5) - the user flagged its absence as a regression, not a deferred nicety.
    //
    // Owns its Bitmap. Disposed by whoever drops it (SkinChangerViewModel), and only once it is off
    // screen: the pixels are native memory the GC doesn't see, so an undisposed screenshot sat around
    // until some unrelated collection, while disposing one that an Image still shows throws on that
    // Image's next redraw (Docs/Internal/avalonia-pitfalls.md, §6). The one place that can still show it
    // after the list has let go is the big-picture window, so disposal waits for that to close.
    public class InteractableScreenshot : IDisposable
    {
        public Bitmap Image { get; }
        public IRelayCommand ShowBigPictureCommand { get; }

        private int _openViewers;
        private bool _disposeRequested;
        private bool _disposed;

        public InteractableScreenshot(Bitmap image)
        {
            Image = image;
            ShowBigPictureCommand = new RelayCommand(ShowBigPicture);
        }

        private void ShowBigPicture()
        {
            if (_disposeRequested)
                return;

            var window = new ImageViewWindow(Image);
            _openViewers++;
            window.Closed += (_, _) =>
            {
                _openViewers--;
                ReleaseIfUnused();
            };
            window.Show();
        }

        public void Dispose()
        {
            _disposeRequested = true;
            ReleaseIfUnused();
        }

        private void ReleaseIfUnused()
        {
            if (!_disposeRequested || _openViewers > 0 || _disposed)
                return;

            _disposed = true;
            Image?.Dispose();
        }
    }
}
