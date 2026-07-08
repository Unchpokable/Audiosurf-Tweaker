using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
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

        private static string EncodePng(Bitmap bitmap)
        {
            using (var memory = new MemoryStream())
            {
                // Bitmap.Save() needs a seekable stream (GDI+ writes headers after the data).
                bitmap.Save(memory, ImageFormat.Png);
                return Convert.ToBase64String(memory.ToArray());
            }
        }

        private static Bitmap DecodePng(string base64)
        {
            using (var memory = new MemoryStream(Convert.FromBase64String(base64)))
            using (var streamedImage = Image.FromStream(memory))
            {
                return new Bitmap(streamedImage);
            }
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
