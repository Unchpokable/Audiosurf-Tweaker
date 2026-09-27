using System;
using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using SkiaSharp;

namespace TweakerUI.Core
{
    internal static class Extensions
    {
        // Width every skin screenshot is kept at in memory (the list's preview panel, the hover tooltip and
        // the big-picture window all draw from it). Height follows the source's aspect - a fixed 860x440 used
        // to squash 16:9 screenshots to 1.95:1.
        public const int ScreenshotWidth = 860;

        // Writes the pixels straight into an Avalonia bitmap's own buffer - one copy, converted to BGRA on the
        // way (SKPixmap.ReadPixels) if the source is anything else; the SKBitmap stays the caller's.
        // This used to be a PNG encode + decode round trip: three copies alive at once and a quality-100 PNG
        // encode per screenshot, which dominated Fast Previews' fill time.
        // A WriteableBitmap, not Bitmap's (format, data, size, ...) constructor: that one goes through
        // SKBitmap.Copy(), which leaves an undisposed shader holding a full copy of the pixels - every
        // bitmap made that way cost twice its size until the next GC (Docs/Internal/avalonia-pitfalls.md, §6).
        // Drawing a WriteableBitmap wraps its pixels in an image without copying and caches that until the
        // next Lock(), so being "writeable" costs nothing here.
        public static Bitmap ToAvaloniaBitmap(this SKBitmap bitmap)
        {
            // Opaque (every JPEG) is passed on as such - same pixels as premultiplied, but it tells the
            // renderer there is nothing to blend.
            var opaque = bitmap.AlphaType == SKAlphaType.Opaque;
            var result = new WriteableBitmap(new PixelSize(bitmap.Width, bitmap.Height), new Vector(96, 96), PixelFormat.Bgra8888, opaque ? AlphaFormat.Opaque : AlphaFormat.Premul);
            using (var target = result.Lock())
            using (var source = bitmap.PeekPixels())
            {
                var targetInfo = new SKImageInfo(bitmap.Width, bitmap.Height, SKColorType.Bgra8888, opaque ? SKAlphaType.Opaque : SKAlphaType.Premul);
                if (!source.ReadPixels(targetInfo, target.Address, target.RowBytes))
                {
                    result.Dispose();
                    throw new InvalidOperationException($"Could not convert a {bitmap.ColorType}/{bitmap.AlphaType} bitmap to BGRA");
                }
            }
            return result;
        }

        // A skin screenshot as kept in memory: ScreenshotWidth wide, aspect preserved, never upscaled.
        public static Bitmap ToScreenshot(this SKBitmap source)
        {
            if (source.Width <= ScreenshotWidth)
                return source.ToAvaloniaBitmap();

            var height = Math.Max(1, (int)Math.Round((double)source.Height * ScreenshotWidth / source.Width));
            using var small = source.Rescale(ScreenshotWidth, height);
            return small.ToAvaloniaBitmap();
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
