using Microsoft.Toolkit.Uwp.Notifications;
using SkiaSharp;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;


namespace SkinChangerRestyle.Core.Extensions
{

    public static class Extensions
    {
        [DllImport("gdi32.dll", EntryPoint = "DeleteObject")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool DeleteObject([In] IntPtr hObject);

        // GDI+ Bitmap path survives only for the resx-embedded UI icons (Properties.Resources.*) -
        // the resx generator hardcodes System.Drawing.Bitmap. Dies with WPF in the Avalonia phase.
        public static ImageSource ImageSourceFromBitmap(Bitmap bmp)
        {
            var handle = bmp.GetHbitmap();
            try
            {
                return Imaging.CreateBitmapSourceFromHBitmap(handle, IntPtr.Zero, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
            }
            finally
            {
                DeleteObject(handle);
            }
        }

        public static ImageSource ToImageSource(this Bitmap bitmapSource)
        {
            return ImageSourceFromBitmap(bitmapSource);
        }

        public static ImageSource ToImageSource(this SKBitmap bitmap)
        {
            using (var image = SKImage.FromBitmap(bitmap))
            using (var data = image.Encode(SKEncodedImageFormat.Png, 100))
            using (var memory = new MemoryStream(data.ToArray()))
            {
                var bitmapImage = new BitmapImage();
                bitmapImage.BeginInit();
                bitmapImage.CacheOption = BitmapCacheOption.OnLoad;
                bitmapImage.StreamSource = memory;
                bitmapImage.EndInit();
                bitmapImage.Freeze();
                return bitmapImage;
            }
        }

        public static SKBitmap Rescale(this SKBitmap source, int newX, int newY)
        {
            return source.Resize(new SKSizeI(newX, newY), SKSamplingOptions.Default);
        }

        public static System.Windows.Size ScaleWidth(this System.Windows.Size origin, float scaleFactor)
        {
            return new System.Windows.Size(origin.Width * scaleFactor, origin.Height);
        }

        public static System.Windows.Size ScaleHeight(this System.Windows.Size origin, float scaleFactor)
        {
            return new System.Windows.Size(origin.Width, origin.Height * scaleFactor);
        }

        public static System.Windows.Size Scale(this System.Windows.Size origin, float scaleFactor)
        {
            return new System.Windows.Size(origin.Width * scaleFactor, origin.Height * scaleFactor);
        }

        public static bool UnorderedSequenceEquals<TElem>(this IList<TElem> origin, IList<TElem> compareWith)
        {
            var hashset = new HashSet<TElem>(origin);

            return origin.Count == compareWith.Count && compareWith.All(hashset.Contains);
        }

        public static Dictionary<TKey,TValue> MergedWith<TKey, TValue>(this Dictionary<TKey, TValue> origin, params KeyValuePair<TKey, TValue>[] extend)
        {
            if (extend == null)
                return origin;

            return origin.MergedWith(extend.ToDictionary(x => x.Key, x => x.Value));
        }

        public static Dictionary<TKey, TValue> MergedWith<TKey, TValue>(this Dictionary<TKey, TValue> origin, Dictionary<TKey, TValue> extend)
        {
            if (extend == null)
                return origin;

            foreach (var pair in extend)
            {
                if (!origin.Contains(pair))
                    origin.Add(pair.Key, pair.Value);
            }

            return origin;
        }

        public static bool SameWith<TItem>(this TItem item, params TItem[] matches)
            where TItem : IComparable<TItem>, IComparable
        {
            return matches.Any(match => match.Equals(item));
        }

        public static System.Windows.Media.Color ToNegative(this System.Windows.Media.Color color)
        {
            return System.Windows.Media.Color.FromArgb(color.A, (byte)(255 - color.R), (byte)(255 - color.G), (byte)(255 - color.B));
        }

        public static void RemoveIf<T>(this Collection<T> source, Predicate<T> predicate)
        {
            foreach (var item in source)
            {
                if (predicate(item))
                {
                    source.Remove(item);
                    return; // Removes only first entry of item
                }
            }
        }
    }
}
