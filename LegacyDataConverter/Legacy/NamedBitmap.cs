using System;
using System.Drawing;
using System.Drawing.Imaging;

namespace ChangerAPI.Engine
{
    /// <summary>
    /// Frozen field-layout copy of ChangerAPI.Engine.NamedBitmap as it existed while skins were
    /// still BinaryFormatter-serialized. Do not "clean up" or evolve this - it exists only so old
    /// .tasp/.askin2 files remain deserializable. See LegacyBinder.
    /// </summary>
    [Serializable]
    internal class NamedBitmap
    {
        public string Name;

        [NonSerialized] public ImageFormat DefaultFormat = ImageFormat.Png;

        private Bitmap source;
        private string format;

        public Bitmap Source => source;
        public string Format => format;
    }
}
