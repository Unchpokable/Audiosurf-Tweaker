using System;

namespace ChangerAPI.Engine
{
    /// <summary>
    /// Frozen field-layout copy of ChangerAPI.Engine.UID as it existed while skins were still
    /// BinaryFormatter-serialized. Do not "clean up" or evolve this - it exists only so old
    /// .tasp/.askin2 files remain deserializable. See LegacyBinder.
    /// </summary>
    [Serializable]
    internal struct UID
    {
        public uint CreationTime { get; set; }
        public Guid uID { get; set; }

        public override string ToString()
        {
            return $"{CreationTime}::{uID}";
        }
    }
}
