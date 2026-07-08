using SkiaSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace SkinChangerRestyle.Core
{
    internal class LoadingCache : IDisposable
    {
        public LoadingCache()
        {
            Data = new List<LoadedSkinData>();
        }

        public List<LoadedSkinData> Data { get; private set; }

        private static string _fileName = "load.cache";
        private static readonly Logger _logger = new Logger();
        private bool _disposedValue;

        public bool Serialize(string path)
        {
            try
            {
                var dto = new CacheDto
                {
                    Entries = Data.Select(entry => new LoadedSkinDataDto
                    {
                        Name = entry.Name,
                        PathToOriginFile = entry.PathToOriginFile,
                        ScreenshotsPng = entry.Screenshots.Select(EncodePng).ToList()
                    }).ToList()
                };

                File.WriteAllText(path + "//" + _fileName, JsonSerializer.Serialize(dto));
                return true;
            }
            catch (Exception ex)
            {
                _logger.Log("LoadingCache", $"Failed to save loading cache to '{path}': {ex}");
                return false;
            }
        }

        public static LoadingCache Find(string path)
        {
            if (TryFind(path, out LoadingCache cache))
                return cache;
            return null;
        }

        public static bool TryFind(string path, out LoadingCache cache)
        {
            cache = null;
            try
            {
                foreach (var file in Directory.EnumerateFiles(path))
                {
                    if (Path.GetFileName(file) == _fileName)
                    {
                        var dto = JsonSerializer.Deserialize<CacheDto>(File.ReadAllText(file));
                        cache = new LoadingCache
                        {
                            Data = dto.Entries.Select(entry => new LoadedSkinData
                            {
                                Name = entry.Name,
                                PathToOriginFile = entry.PathToOriginFile,
                                Screenshots = entry.ScreenshotsPng.Select(DecodePng).ToArray()
                            }).ToList()
                        };
                        return true;
                    }
                }
                return false;
            }
            catch (JsonException ex)
            {
                _logger.Log("LoadingCache", $"Failed to load loading cache from '{path}': {ex}");
                return false;
            }
        }

        private static string EncodePng(SKBitmap bitmap)
        {
            using (var image = SKImage.FromBitmap(bitmap))
            using (var data = image.Encode(SKEncodedImageFormat.Png, 100))
            {
                return Convert.ToBase64String(data.ToArray());
            }
        }

        private static SKBitmap DecodePng(string base64)
        {
            return SKBitmap.Decode(Convert.FromBase64String(base64));
        }

        private class CacheDto
        {
            public List<LoadedSkinDataDto> Entries { get; set; }
        }

        private class LoadedSkinDataDto
        {
            public string Name { get; set; }
            public string PathToOriginFile { get; set; }
            public List<string> ScreenshotsPng { get; set; }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposedValue)
            {
                if (disposing)
                {
                    Data.ForEach(x => x.Dispose());
                }
                Data = null;
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
