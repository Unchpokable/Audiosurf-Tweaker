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

        public static Bitmap Rescale(this Bitmap source, int newX, int newY)
        {
            return new Bitmap((Image)source, newX, newY);
        }

        public static Bitmap Rescale(this Bitmap source, float scaleX, float scaleY)
        {
            return new Bitmap(source, (int)(source.Width * scaleX), (int)(source.Height * scaleY));
        }
    }
}
