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

        // TweakerCore.Utilities.Extensions.Rescale only takes an SKSizeI - this int,int overload matches
        // the old SkinChangerRestyle.Core.Extensions call shape used throughout the Skin Changer port.
        // SKSamplingOptions.Default is nearest-neighbor (SKFilterMode defaults to its 0 value, Nearest),
        // which looked visibly blocky downscaling full-size skin screenshots down to thumbnail/preview
        // sizes - linear filtering with mipmaps gives a much cleaner downscale.
        public static SKBitmap Rescale(this SKBitmap source, int width, int height)
        {
            var sampling = new SKSamplingOptions(SKFilterMode.Linear, SKMipmapMode.Linear);
            return source.Resize(new SKSizeI(width, height), sampling);
        }
    }
}
