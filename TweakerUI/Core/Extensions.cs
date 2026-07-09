using System.IO;
using Avalonia.Media.Imaging;
using SkiaSharp;

namespace TweakerUI.Core
{
    internal static class Extensions
    {
        // WPF's ToImageSource() went through a resx System.Drawing.Bitmap and a BitmapImage; the Avalonia
        // equivalent is an in-memory PNG round-trip, since Bitmap(Stream) has no direct SKBitmap overload.
        public static Bitmap ToAvaloniaBitmap(this SKBitmap bitmap)
        {
            using var image = SKImage.FromBitmap(bitmap);
            using var data = image.Encode(SKEncodedImageFormat.Png, 100);
            using var stream = new MemoryStream();
            data.SaveTo(stream);
            stream.Position = 0;
            return new Bitmap(stream);
        }
    }
}
