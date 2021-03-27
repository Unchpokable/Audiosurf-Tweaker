#pragma warning disable CS1717

namespace Audiosurf_SkinChanger.Utilities
{
    using System;
    using System.Runtime.Serialization.Formatters.Binary;
    using System.Runtime.Serialization;
    using System.IO;
    using Audiosurf_SkinChanger.Engine;

    [Serializable]
    class SkinInfo
    {
        public int Hash;
        public string Name;

        public SkinInfo(int hash, string name)
        {
            Name = name;
            Hash = hash;
        }

        public SkinInfo(AudiosurfSkin skin)
        {
            Name = skin.Name;
        }

        public void Compile(string path)
        {
            path = path;
            using (var memoryStream = File.Exists(path) ? new FileStream(path, FileMode.Create) : new FileStream(path, FileMode.OpenOrCreate))
            {
                IFormatter formatter = new BinaryFormatter();
                formatter.Serialize(memoryStream, this);
            }
        }


        public override string ToString()
        {
            return Name;
        }

        public static SkinInfo Load(string path)
        {
            if (!File.Exists(path))
                return null;
            using (var fileStream = new FileStream(path, FileMode.Open))
            {
                try
                {
                    var obj = (SkinInfo)new BinaryFormatter().Deserialize(fileStream);
                    return obj;
                }
                catch { return null; }
            }
        }
    }
}
