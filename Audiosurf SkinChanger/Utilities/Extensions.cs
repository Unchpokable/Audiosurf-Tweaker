namespace Audiosurf_SkinChanger.Utilities
{
    using System.Drawing;
    using System.Windows.Forms;
    using System;
    using System.Collections.Generic;

    public static class Extensions
    {
        public static Bitmap Rescale(this Bitmap source, int newWidth, int newHeight)
        {
            return new Bitmap((Bitmap)source, new Size(newWidth, newHeight));
        }

        public static void ClearAll(this PictureBox[] source)
        {
            foreach(var pic in source)
            {
                pic.Image = null;
                pic.Invalidate();
            }
        }

        public static void ForEach<TSource>(this IList<TSource> source, Action<TSource> action)
        {
            if (source == null)
                throw new NullReferenceException($"{nameof(source)}: Object reference not set to an instance of an object");

            if (action == null)
                throw new NullReferenceException($"{nameof(action)}: Object reference not set to an instance of an object");

            for (int i = 0; i < source.Count; i++)
            {
                action(source[i]);
            }
        }
    }
}
