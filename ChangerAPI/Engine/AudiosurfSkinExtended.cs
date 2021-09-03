namespace ChangerAPI.Engine
{
    using System;
    using System.IO;
    using ChangerAPI.Utilities;
    using System.Runtime.Serialization;
    using System.Runtime.Serialization.Formatters.Binary;

    [Serializable]
    public class AudiosurfSkinExtended : AudiosurfSkin
    {
        public NamedBitmap Cover { get; set; }
        public string ID => id.ToString();

        private UID id;

        public AudiosurfSkinExtended() : base()
        {
            Cover = new NamedBitmap();
        }

        public static AudiosurfSkinExtended Reinterpret(AudiosurfSkin source)
        {
            var tempSkin = new AudiosurfSkinExtended();
            tempSkin.Name = source.Name;
            tempSkin.SkySpheres = source.SkySpheres;
            tempSkin.SkySphereSource = source.SkySphereSource;
            tempSkin.Particles = source.Particles;
            tempSkin.Cliffs = source.Cliffs;
            tempSkin.Hits = source.Hits;
            tempSkin.Tiles = source.Tiles;
            tempSkin.TilesFlyup = source.TilesFlyup;
            tempSkin.Rings = source.Rings;
            tempSkin.Previews = source.Previews;

            var id = new UID((uint)DateTime.Now.Ticks);
            tempSkin.id = id;

            return tempSkin;
        }

        private struct UID
        {
            public uint CreationTime { get; }
            public Guid uID { get; }

            public UID(uint cTime)
            {
                CreationTime = cTime;
                uID = Guid.NewGuid();
            }

            public override string ToString()
            {
                return $"{CreationTime}::{uID}";
            }
        }
    }
}
