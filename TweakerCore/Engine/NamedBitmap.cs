using System;
using System.Linq;
using SkiaSharp;
using TweakerCore.Utilities;

namespace TweakerCore.Engine
{

    public class NamedBitmap : IDisposable
    {
        public int Width => source.Width;
        public int Height => source.Height;
        public string Name;

        public ImageInfo Info => new ImageInfo(format, Name);

        private SKBitmap source;
        private string format;

        private bool disposedValue;

        private const int JpegEncodeQuality = 95;

        public NamedBitmap()
        {
        }

        public NamedBitmap(SKBitmap source)
        {
            this.source = source;
        }

        public NamedBitmap(string name, SKBitmap source)
        {
            Name = name;
            this.source = source;
            format = ProcessImageFormat(name).ToString();
        }

        public NamedBitmap(string path, ImageInfo imageInfo)
        {
            source = SKBitmap.Decode(path);
            Name = imageInfo.FileName;
            format = imageInfo.Format;
        }

        public NamedBitmap(SKBitmap original, ImageInfo imageInfo)
        {
            source = original;
            Name = imageInfo.FileName;
            format = imageInfo.Format;
        }

        public void Apply(Func<SKBitmap, SKBitmap> transform)
        {
            source = transform(source);
        }

        public NamedBitmap DeepClone()
        {
            return new NamedBitmap(Name, source.Copy());
        }

        private SKEncodedImageFormat ProcessImageFormat(string srcFileName)
        {
            return GetImageFormatByExtension(srcFileName.Split('.').Last());
        }

        public void SetImage(SKBitmap source)
        {
            this.source = source;
        }

        public void SetImage(NamedBitmap other)
        {
            this.source = other.source;
            this.Name = other.Name;
            this.format = other.format;
        }

        private SKEncodedImageFormat GetImageFormatByExtension(string extension)
        {
            switch (extension.ToLowerInvariant())
            {
                case "png":
                    return SKEncodedImageFormat.Png;
                case "jpg":
                case "jpeg":
                    return SKEncodedImageFormat.Jpeg;
                // Skia can't encode BMP; the skin file masks only ever admit .png/.jpg,
                // so anything else falls back to PNG rather than failing.
                default:
                    return SKEncodedImageFormat.Png;
            }
        }

        public static explicit operator SKBitmap(NamedBitmap obj)
        {
            return obj.source;
        }

        public static implicit operator NamedBitmap(SKBitmap obj)
        {
            return new NamedBitmap(obj);
        }

        public void Save(string filepath)
        {
            if (source == null)
                return;

            using (var image = SKImage.FromBitmap(source))
            using (var data = image.Encode(GetImageFormatByExtension(format.ToLower()), JpegEncodeQuality))
            using (var filestream = System.IO.File.Create(filepath + @"\\" + Name))
            {
                data.SaveTo(filestream);
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    source?.Dispose();
                }

                disposedValue = true;
            }
        }

        public void Dispose()
        {
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }
    }
}
