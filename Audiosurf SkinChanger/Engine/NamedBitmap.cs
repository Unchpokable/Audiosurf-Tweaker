namespace Audiosurf_SkinChanger.Engine
{
    using System.Drawing;
    using System;
    using System.IO;
    using System.Drawing.Imaging;

    [Serializable]
    public class NamedBitmap
    {
        private Bitmap Source;
        public string Name;
        private string format;

        public NamedBitmap(Image original)
        {
            Source = new Bitmap(original);
        }

        public NamedBitmap(string name, Image source)
        {
            Name = name;
            Source = new Bitmap(source);
            format = ProcessImageFormat(name).ToString();
        }

        public NamedBitmap(string name, Bitmap source)
        {
            Name = name;
            Source = source;
            format = ProcessImageFormat(name).ToString();
        }

        private ImageFormat ProcessImageFormat(string srcFileName)
        {
            return GetImageFormatByExtension(srcFileName.Split('.')[0]);
        }
        
        private ImageFormat GetImageFormatByExtension(string extension)
        {
            switch (extension)
            {
                case "bmp":
                    return ImageFormat.Bmp;
                case "png":
                    return ImageFormat.Png;
                case "jpg":
                    return ImageFormat.Jpeg;
                default:
                    return ImageFormat.Jpeg;
            }
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
            Source.Save(filepath + @"\\" + Name, GetImageFormatByExtension(format.ToLower()));
        }
    }
}
