using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Formatters.Binary;
using System.IO;

namespace SkinChangerRestyle.Core
{
    [Serializable]
    internal class LoadingCache : IDisposable
    {
        public LoadingCache()
        {
            Data = new List<LoadedSkinData>();
        }

        public List<LoadedSkinData> Data { get; private set; }

        private static string _fileName = "load.cache";
        private bool disposedValue;

        public bool Serialize(string path)
        {
            try
            {
                var formatter = new BinaryFormatter();
                using (var stream = new FileStream(path + "//" + _fileName, FileMode.Create))
                {
                    formatter.Serialize(stream, this);
                    return true;
                }
            }
            catch (Exception)
            {
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
                        var formatter = new BinaryFormatter();
                        using (var fileStream = new FileStream(file, FileMode.Open))
                            cache = (LoadingCache)formatter.Deserialize(fileStream);
                        return true;
                    }
                }
                return false;
            }
            catch (SerializationException)
            {
                return false;
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    Data.ForEach(x => x.Dispose());
                }
                Data = null;
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
