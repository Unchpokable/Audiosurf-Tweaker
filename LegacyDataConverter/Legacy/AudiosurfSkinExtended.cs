using System;

namespace ChangerAPI.Engine
{
    /// <summary>
    /// Frozen field-layout copy of ChangerAPI.Engine.AudiosurfSkinExtended as it existed while skins
    /// were still BinaryFormatter-serialized. Do not "clean up" or evolve this - it exists only so
    /// old .tasp/.askin2 files remain deserializable. See LegacyBinder.
    /// </summary>
    [Serializable]
    internal class AudiosurfSkinExtended : AudiosurfSkin
    {
        public NamedBitmap Cover { get; set; }

        private UID _id;

        public string ID => _id.ToString();
    }
}
