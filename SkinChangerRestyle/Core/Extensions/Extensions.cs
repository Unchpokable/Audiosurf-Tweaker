using Microsoft.Toolkit.Uwp.Notifications;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
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

        public static Bitmap Rescale(this Bitmap source, int newX, int newY)
        {
            var newBmp = new Bitmap(source, newX, newY);
            return newBmp;
        }

        public static Bitmap Rescale(this Bitmap source, float scaleX, float scaleY)
        {
            return new Bitmap(source, (int)(source.Width * scaleX), (int)(source.Height * scaleY));
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

        public static System.Windows.Media.Color ToMediaColor(this System.Drawing.Color color)
        {
            return new System.Windows.Media.Color() { R = color.R, G = color.G, B = color.B, A = color.A };
        }

        public static System.Drawing.Color ToNegative(this System.Drawing.Color color)
        {
            return System.Drawing.Color.FromArgb(color.A, 255 - color.R, 255 - color.G, 255 - color.B);
        } 
        
        public static System.Windows.Media.Color ToNegative(this System.Windows.Media.Color color)
        {
            return System.Windows.Media.Color.FromArgb(color.A, (byte)(255 - color.R), (byte)(255 - color.G), (byte)(255 - color.B));
        }

        #region not extensions
        public static async void DisposeAndClear(params IDisposable[] disposable)
        {
            foreach (var d in disposable)
                d?.Dispose();

            await Task.Run(async () =>
            {
                await Task.Delay(1000);
                // Ye, i know that manual calling GC.Collct() is a very bad practice, but idk why, in this certain case GC works as shit bag and lefts OVER NINE THOUSANDS unused memory for an undefined long while
                GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced, false, true);
                GC.WaitForPendingFinalizers();
                GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced, false, true);
            });
        }

        public static void Cmd(string command)
        {
            try
            {
                System.Diagnostics.Process.Start(new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = $"/c {command}",
                    WindowStyle = ProcessWindowStyle.Hidden,
                });
            } 
            catch { }
        }

        public static void ShowUWPNotification(string caption, string message)
        {
            var toast = new ToastContentBuilder()
                .AddHeader("0", caption, new ToastArguments())
                .SetToastDuration(ToastDuration.Short)
                .AddText(message);

            if (SettingsProvider.IsUWPNotificationSilent)
                toast.AddAudio(new ToastAudio() { Silent = true });
            toast.Show();
        }
        #endregion
    }
}
