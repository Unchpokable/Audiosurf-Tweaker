using System;
using ChangerAPI.Utilities;

namespace ChangerAPI.Engine
{
    /// <summary>
    /// Frozen field-layout copy of ChangerAPI.Engine.AudiosurfSkin as it existed while skins were
    /// still BinaryFormatter-serialized. Do not "clean up" or evolve this - it exists only so old
    /// .tasp/.askin2 files remain deserializable. See LegacyBinder.
    /// </summary>
    [Serializable]
    internal class AudiosurfSkin
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
    }
}
