using System;
using System.IO;
using ChangerAPI.Utilities;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Formatters.Binary;

namespace ChangerAPI.Engine
{

    [Serializable]
    public class AudiosurfSkin : IDisposable
    {
        private bool disposedValue;

        public string Source { get; set; }
        public string Name { get; set; }
        public ImageGroup SkySpheres { get; set; }
        public ImageGroup SkySphereSource { get; set; }
        public ImageGroup Cliffs { get; set; }
        public ImageGroup Hits { get; set; }
        public NamedBitmap Tiles { get; set; }
        public NamedBitmap TilesFlyup { get; set; }
        public ImageGroup Particles { get; set; }
        public ImageGroup Rings { get; set; }
        public ImageGroup Previews { get; set; }

        public AudiosurfSkin()
        {
            SkySpheres = new ImageGroup();
            Cliffs = new ImageGroup();
            Hits = new ImageGroup();
            Previews = new ImageGroup();
            Particles = new ImageGroup();
            Rings = new ImageGroup();
            Tiles = new NamedBitmap();
            TilesFlyup = new NamedBitmap();
        }

        public AudiosurfSkin Clone()
        {
            return new AudiosurfSkin()
            {
                Source = this.Source,
                Name = this.Name,
                SkySpheres = this.SkySpheres,
                Cliffs = this.Cliffs,
                Hits = this.Hits,
                Tiles = this.Tiles,
                TilesFlyup = this.TilesFlyup,
                Particles = this.Particles,
                Rings = this.Rings
            };
        }

        public AudiosurfSkin DeepClone()
        {
            using (var memory = new MemoryStream())
            {
                IFormatter formatter = new BinaryFormatter();
                formatter.Serialize(memory, this);
                memory.Position = 0;
                return (AudiosurfSkin)formatter.Deserialize(memory);
            }
        }

        public override string ToString()
        {
            return Name;
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    SkySpheres.Dispose();
                    Cliffs.Dispose();
                    Hits.Dispose();
                    Tiles.Dispose();
                    Rings.Dispose();
                    Previews.Dispose();
                }
                SkySpheres = null;
                Cliffs = null;
                Tiles = null;
                Rings = null;
                Hits = null;
                Previews = null;
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
