using System;

namespace ChangerAPI.Engine
{
    [Serializable]
    internal struct UID
    {
        public uint CreationTime { get; }
        public Guid uID { get; }

        public UID(uint cTime)
        {
            CreationTime = cTime;
            uID = Guid.NewGuid();
        }

        public override string ToString()
        {
            return $"{CreationTime}::{uID}";
        }
    }
}
