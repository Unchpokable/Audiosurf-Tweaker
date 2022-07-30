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
    internal class LoadingCache
    {
        public LoadingCache()
        {
            _formatter = new BinaryFormatter();
        }

        public List<LoadedSkinData> Data { get; private set; }

        private IFormatter _formatter;

        private string _fileName = "load.cache";

        public bool Serialize(string path)
        {
            try
            {
                using (var stream = new FileStream(path + _fileName, FileMode.Create))
                {
                    _formatter.Serialize(stream, this);
                    return true;
                }
            }
            catch (Exception e)
            {
                return false;
            }
        }

        public static LoadingCache Find(string path)
        {
            return null;
        }

        public static bool TryFind(string path, out LoadingCache cache)
        {
            cache = null;
            return false;
        }
    }
}
