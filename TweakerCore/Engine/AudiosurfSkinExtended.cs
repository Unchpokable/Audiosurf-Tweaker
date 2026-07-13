using System;

namespace TweakerCore.Engine
{

    [Serializable]
    public class AudiosurfSkinExtended : AudiosurfSkin, IDisposable
    {
        public NamedBitmap Cover { get; set; }
        public string ID => _id.ToString();

        private UID _id;

        public AudiosurfSkinExtended() : base()
        {
            Cover = new NamedBitmap();
            // Reinterpret/Clone/DeepClone all already assign a fresh id on creation - the default
            // constructor (used by CreateSkinFromFolder and every ReadSkinArchive decompile) was the
            // only path that left this at its zero default, defeating "Import from Game"/every
            // decompiled skin's uniqueness.
            _id = new UID((uint)DateTime.Now.Ticks);
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
            tempSkin._id = id;

            return tempSkin;
        }

        public new AudiosurfSkinExtended DeepClone()
        {
            return new AudiosurfSkinExtended
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
                Previews = Previews?.DeepClone(),
                Cover = Cover?.DeepClone(),
                _id = new UID((uint)DateTime.Now.Ticks)
            };
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
                _id = new UID((uint)DateTime.Now.Ticks)
            };
        }

        // Base Dispose(bool) never touches Cover - it's a member only this subclass adds.
        protected override void Dispose(bool disposing)
        {
            if (disposing)
                Cover?.Dispose();
            Cover = null;
            base.Dispose(disposing);
        }
    }
}
