using System;

namespace TweakerCore.Engine
{
    public class AudiosurfSkinExtended : AudiosurfSkin
    {
        public NamedBitmap Cover { get; set; }

        // Written into the archive manifest as its Uid. Assigned on construction so every path that
        // produces a skin - decompile, "Import from Game", DeepClone - gets a distinct one; nothing
        // reads it back, so an id from an older manifest is simply replaced on the next write.
        public string ID { get; private set; } = Guid.NewGuid().ToString();

        public AudiosurfSkinExtended()
        {
            Cover = new NamedBitmap();
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
                Cover = Cover?.DeepClone()
            };
        }

        // Base Dispose never touches Cover - it's a member only this subclass adds.
        public override void Dispose()
        {
            Cover?.Dispose();
            Cover = null;
            base.Dispose();
        }
    }
}
