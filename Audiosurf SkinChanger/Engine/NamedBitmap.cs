namespace Audiosurf_SkinChanger.Engine
{
    using System.Drawing;
    using System;
    using System.IO;

    [Serializable]
    public class NamedBitmap
    {
        private Bitmap Source;
        public string Name;

        public NamedBitmap(Image original)
        {
            Source = new Bitmap(original);
        }

        public NamedBitmap(string name, Image source)
        {
            Name = name;
            Source = new Bitmap(source);
        }

        public NamedBitmap(string name, Bitmap source)
        {
            Name = name;
            Source = source;
        }

        public static explicit operator Bitmap(NamedBitmap obj)
        {
            return obj.Source;
        }

        public static implicit operator Image(NamedBitmap obj)
        {
            return obj.Source;
        }

        public void Save(string filepath)
        {
            Source.Save(filepath + @"\\" + Name);
        }
    }
}
