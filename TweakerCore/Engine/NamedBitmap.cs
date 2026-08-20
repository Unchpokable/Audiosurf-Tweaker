using System;
using System.IO;
using SkiaSharp;

namespace TweakerCore.Engine
{
    public class NamedBitmap : IDisposable
    {
        public string Name;

        private SKBitmap _source;
        private SKEncodedImageFormat _format = SKEncodedImageFormat.Png;

        private const int JpegEncodeQuality = 95;

        public NamedBitmap()
        {
        }

        public NamedBitmap(SKBitmap source)
        {
            _source = source;
        }

        public NamedBitmap(string name, SKBitmap source)
        {
            Name = name;
            _source = source;
            _format = GetImageFormat(name);
        }

        public NamedBitmap DeepClone()
        {
            return new NamedBitmap(Name, _source.Copy());
        }

        public void SetImage(SKBitmap source)
        {
            _source = source;
        }

        // Skia can't encode BMP; the skin file masks only ever admit .png/.jpg,
        // so anything else falls back to PNG rather than failing.
        internal static SKEncodedImageFormat GetImageFormat(string fileName)
        {
            var extension = Path.GetExtension(fileName).ToLowerInvariant();
            return extension == ".jpg" || extension == ".jpeg"
                ? SKEncodedImageFormat.Jpeg
                : SKEncodedImageFormat.Png;
        }

        public static explicit operator SKBitmap(NamedBitmap obj)
        {
            return obj._source;
        }

        public static implicit operator NamedBitmap(SKBitmap obj)
        {
            return new NamedBitmap(obj);
        }

        public void Save(string filepath)
        {
            if (_source == null)
                return;

            // Name can come straight from a decompiled skin's manifest.json, which is untrusted
            // (skins are shared user files) - strip any directory component so a crafted manifest
            // entry like "..\..\Startup\x.png" can't write outside the target folder.
            var safeName = Path.GetFileName(Name);

            using (var image = SKImage.FromBitmap(_source))
            using (var data = image.Encode(_format, JpegEncodeQuality))
            using (var filestream = File.Create(Path.Combine(filepath, safeName)))
            {
                data.SaveTo(filestream);
            }
        }

        public void Dispose()
        {
            _source?.Dispose();
            _source = null;
        }
    }
}
