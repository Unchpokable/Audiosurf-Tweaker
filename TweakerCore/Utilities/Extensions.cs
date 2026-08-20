using SkiaSharp;

namespace TweakerCore.Utilities
{
    public static class Extensions
    {
        public static SKBitmap Rescale(this SKBitmap source, SKSizeI newSize)
        {
            return source.Resize(newSize, SKSamplingOptions.Default);
        }
    }
}
