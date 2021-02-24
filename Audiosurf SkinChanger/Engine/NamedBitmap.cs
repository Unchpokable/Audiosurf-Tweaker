namespace Audiosurf_SkinChanger.Engine
{
    using System.Drawing;
    using System;
    using System.Linq;
    using System.Drawing.Imaging;
    using Audiosurf_SkinChanger.Skin_Creator;

    [Serializable]
    public class NamedBitmap
    {
        public int Width => Source.Width;
        public int Height => Source.Height;
        public Size Size => new Size(Width, Height);
        

        private Bitmap Source;
        public string Name;
        private string format;

        [NonSerialized]
        public ImageFormat DefaultFormat = ImageFormat.Png;

        public NamedBitmap()
        {
        }

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

        public NamedBitmap(string path, ImageInfo imageInfo)
        {
            Source = (Bitmap)Image.FromFile(path);
            Name = imageInfo.FileName;
            format = imageInfo.Format;
        }

        public NamedBitmap(Image original, ImageInfo imageInfo)
        {
            Source = (Bitmap)original;
            Name = imageInfo.FileName;
            format = imageInfo.Format;
        }

        public void Apply(Func<Bitmap, Bitmap> transform)
        {
            Source = transform(Source);
        }

        private ImageFormat ProcessImageFormat(string srcFileName)
        {
            return GetImageFormatByExtension(srcFileName.Split('.').Last());
        }

        public void SetImage(Bitmap source)
        {
            Source = source;
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
                    return ImageFormat.Png;
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

        public static implicit operator NamedBitmap(Bitmap obj)
        {
            return new NamedBitmap(obj);
        }

        public void Save(string filepath)
        {
            Source.Save(filepath + @"\\" + Name, GetImageFormatByExtension(format.ToLower()));
        }
    }
}
