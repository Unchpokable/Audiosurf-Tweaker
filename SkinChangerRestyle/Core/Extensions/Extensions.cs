namespace SkinChangerRestyle.Core.Extensions
{
    using ChangerAPI.Engine;
    using System;
    using System.Collections.Generic;
    using System.Drawing;
    using System.Linq;
    using System.Runtime.InteropServices;
    using System.Text;
    using System.Threading.Tasks;
    using System.Windows;
    using System.Windows.Interop;
    using System.Windows.Media;
    using System.Windows.Media.Imaging;

    public static class Extensions
    {

        [DllImport("gdi32.dll", EntryPoint = "DeleteObject")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool DeleteObject([In] IntPtr hObject);

        private static ImageSource ImageSourceFromBitmap(Bitmap bmp)
        {
            var handle = bmp.GetHbitmap();
            try
            {
                return Imaging.CreateBitmapSourceFromHBitmap(handle, IntPtr.Zero, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
            }
            finally { DeleteObject(handle); }
        }

        public static ImageSource ToImageSource(this Bitmap bitmapSource)
        {
            return ImageSourceFromBitmap(bitmapSource);
        }

        public static Queue<T> ToQueue<T>(this IEnumerable<T> source)
        {
            var temp = new Queue<T>();
            foreach (var item in source)
                temp.Enqueue(item);
            return temp;
        }

        public static Queue<T> ToQueue<T>(this T[] source)
        {
            var temp = new Queue<T>();
            foreach (var item in source)
                temp.Enqueue(item);
            return temp;
        }
    }
}
