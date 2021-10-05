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
            tempSkin.Cover = new NamedBitmap();
            
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

        public new AudiosurfSkinExtended DeepClone()
        {
            using (var memory = new MemoryStream())
            {
                IFormatter formatter = new BinaryFormatter();
                formatter.Serialize(memory, this);
                memory.Position = 0;
                return (AudiosurfSkinExtended)formatter.Deserialize(memory);
            }
        }

        public new AudiosurfSkinExtended Clone()
        {
            return new AudiosurfSkinExtended()
            {
                Source = this.Source,
                Name = this.Name,
                SkySpheres = this.SkySpheres,
                Cliffs = this.Cliffs,
                Hits = this.Hits,
                Tiles = this.Tiles,
                TilesFlyup = this.TilesFlyup,
                Particles = this.Particles,
                Rings = this.Rings,
                Previews = this.Previews,
                Cover = this.Cover,
                id = new UID((uint)DateTime.Now.Ticks)
            };
        }    
    }
}
