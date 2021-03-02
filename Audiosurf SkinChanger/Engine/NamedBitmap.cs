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
        public int Width => source.Width;
        public int Height => source.Height;
        public Size Size => new Size(Width, Height);
        public string Name;

        private Bitmap source;
        private string format;

        [NonSerialized]
        public ImageFormat DefaultFormat = ImageFormat.Png;

        public NamedBitmap()
        {
        }

        public NamedBitmap(Image original)
        {
            source = new Bitmap(original);
        }

        public NamedBitmap(string name, Image source)
        {
            Name = name;
            this.source = new Bitmap(source);
            format = ProcessImageFormat(name).ToString();
        }

        public NamedBitmap(string name, Bitmap source)
        {
            Name = name;
            this.source = source;
            format = ProcessImageFormat(name).ToString();
        }

        public NamedBitmap(string path, ImageInfo imageInfo)
        {
            source = (Bitmap)Image.FromFile(path);
            Name = imageInfo.FileName;
            format = imageInfo.Format;
        }

        public NamedBitmap(Image original, ImageInfo imageInfo)
        {
            source = (Bitmap)original;
            Name = imageInfo.FileName;
            format = imageInfo.Format;
        }

        public void Apply(Func<Bitmap, Bitmap> transform)
        {
            source = transform(source);
        }

        private ImageFormat ProcessImageFormat(string srcFileName)
        {
            return GetImageFormatByExtension(srcFileName.Split('.').Last());
        }

        public void SetImage(Bitmap source)
        {
            this.source = source;
        }

        public void SetImage(NamedBitmap other)
        {
            this.source = other.source;
            this.Name = other.Name;
            this.format = other.format;
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
            return obj.source;
        }

        public static implicit operator Image(NamedBitmap obj)
        {
            return obj.source;
        }

        public static implicit operator NamedBitmap(Bitmap obj)
        {
            return new NamedBitmap(obj);
        }

        public void Save(string filepath)
        {
            source.Save(filepath + @"\\" + Name, GetImageFormatByExtension(format.ToLower()));
        }
    }
}
