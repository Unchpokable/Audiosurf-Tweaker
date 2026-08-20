using System;
using TweakerCore.Utilities;

namespace TweakerCore.Engine
{
    public class AudiosurfSkin : IDisposable
    {
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

        public AudiosurfSkin DeepClone()
        {
            return new AudiosurfSkin
            {
                Source = Source,
                Name = Name,
                SkySpheres = SkySpheres?.DeepClone(),
                SkySphereSource = SkySphereSource?.DeepClone(),
                Cliffs = Cliffs?.DeepClone(),
                Hits = Hits?.DeepClone(),
                Tiles = Tiles?.DeepClone(),
                TilesFlyup = TilesFlyup?.DeepClone(),
                Particles = Particles?.DeepClone(),
                Rings = Rings?.DeepClone(),
                Previews = Previews?.DeepClone()
            };
        }

        public override string ToString()
        {
            return Name;
        }

        // Every member is null-checked rather than only SkySphereSource: ReadSkinArchive builds
        // instances field by field, so any of them can legitimately be unset.
        public virtual void Dispose()
        {
            SkySpheres?.Dispose();
            SkySphereSource?.Dispose();
            Cliffs?.Dispose();
            Hits?.Dispose();
            Tiles?.Dispose();
            TilesFlyup?.Dispose();
            Rings?.Dispose();
            Previews?.Dispose();
            Particles?.Dispose();

            SkySpheres = null;
            SkySphereSource = null;
            Cliffs = null;
            Tiles = null;
            TilesFlyup = null;
            Rings = null;
            Hits = null;
            Previews = null;
            Particles = null;
        }
    }
}
