using ChangerAPI.Engine;
using System;
using System.Collections.Generic;

namespace ChangerAPI.Utilities
{
    /// <summary>
    /// Frozen field-layout copy of ChangerAPI.Utilities.ImageGroup as it existed while skins were
    /// still BinaryFormatter-serialized. Do not "clean up" or evolve this - it exists only so old
    /// .tasp/.askin2 files remain deserializable. See LegacyBinder.
    /// </summary>
    [Serializable]
    internal class ImageGroup
    {
        public string Name { get; set; }
        public IList<NamedBitmap> Group { get; private set; }

        public ImageGroup()
        {
            Name = "default";
            Group = new List<NamedBitmap>();
        }
    }
}
