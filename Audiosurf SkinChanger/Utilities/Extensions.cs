namespace Audiosurf_SkinChanger.Utilities
{
    using System.Drawing;
    using System.Windows.Forms;
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Reflection;
    using System.Linq.Expressions;

    public static class Extensions
    {
        public delegate void SetPropertySafeDelegate<TResult>(Control @this, Expression<Func<TResult>> property, TResult value);

        public static Bitmap Rescale(this Bitmap source, Size newSize)
        {
            return new Bitmap(source, newSize);
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

        public static Bitmap[] Squarify(this Bitmap spritesheet)
        {
            int widthThird = (int)((double)spritesheet.Width / 2.0 + 0.5);
            int heightThird = (int)((double)spritesheet.Height / 2.0 + 0.5);
            Bitmap[,] bmps = new Bitmap[2, 2];
            for (int i = 0; i < 2; i++)
                for (int j = 0; j < 2; j++)
                {
                    bmps[i, j] = new Bitmap(widthThird, heightThird);
                    Graphics g = Graphics.FromImage(bmps[i, j]);
                    g.DrawImage(spritesheet, new Rectangle(0, 0, widthThird, heightThird), new Rectangle(j * widthThird, i * heightThird, widthThird, heightThird), GraphicsUnit.Pixel);
                    g.Dispose();
                }
            return bmps.Cast<Bitmap>().ToArray();
        }

        public static void SetProperty<TValue>(this Control @this, Expression<Func<TValue>> property, TValue value)
        {
            var propertyInfo = (property.Body as MemberExpression).Member as PropertyInfo;

            if (@this.InvokeRequired)
            {
                @this.Invoke(new SetPropertySafeDelegate<TValue>(SetProperty), new object[] { @this, property, value });
            }
            else
            {
                @this.GetType().InvokeMember(propertyInfo.Name, BindingFlags.SetProperty, null, @this, new object[] { value });
            }
        }
    }
}
