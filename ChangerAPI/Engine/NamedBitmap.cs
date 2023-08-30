using System.Drawing;
using System;
using System.Linq;
using System.Drawing.Imaging;
using ChangerAPI.Utilities;

namespace ChangerAPI.Engine
{

    [Serializable]
    public class NamedBitmap : IDisposable
    {
        public int Width => _source.Width;
        public int Height => _source.Height;
        public Size Size => new Size(Width, Height);
        public string Name;


        [NonSerialized] public ImageFormat DefaultFormat = ImageFormat.Png;
        public ImageInfo Info => new ImageInfo(_format, Name);

        private Bitmap _source;
        private string _format;

        
        private bool _disposedValue;

        public NamedBitmap()
        {
        }

        public NamedBitmap(Image original)
        {
            _source = new Bitmap(original);
        }

        public NamedBitmap(string name, Image source)
        {
            Name = name;
            this._source = new Bitmap(source);
            _format = ProcessImageFormat(name).ToString();
        }

        public NamedBitmap(string name, Bitmap source)
        {
            Name = name;
            this._source = source;
            _format = ProcessImageFormat(name).ToString();
        }

        public NamedBitmap(string path, ImageInfo imageInfo)
        {
            _source = (Bitmap)Image.FromFile(path);
            Name = imageInfo.FileName;
            _format = imageInfo.Format;
        }

        public NamedBitmap(Image original, ImageInfo imageInfo)
        {
            _source = (Bitmap)original;
            Name = imageInfo.FileName;
            _format = imageInfo.Format;
        }

        public void Apply(Func<Bitmap, Bitmap> transform)
        {
            _source = transform(_source);
        }

        private ImageFormat ProcessImageFormat(string srcFileName)
        {
            return GetImageFormatByExtension(srcFileName.Split('.').Last());
        }

        public void SetImage(Bitmap source)
        {
            this._source = source;
        }

        public void SetImage(NamedBitmap other)
        {
            this._source = other._source;
            this.Name = other.Name;
            this._format = other._format;
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
            return obj._source;
        }

        public static implicit operator Image(NamedBitmap obj)
        {
            return obj._source;
        }

        public static implicit operator NamedBitmap(Bitmap obj)
        {
            return new NamedBitmap(obj);
        }

        public void Save(string filepath)
        {
            _source?.Save(filepath + @"\\" + Name, GetImageFormatByExtension(_format.ToLower()));
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposedValue)
            {
                if (disposing)
                {
                    _source?.Dispose();
                }

                _disposedValue = true;
            }
        }

        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
    }
}
