using System.Net;

namespace SkinChangerRestyle.Core.NetworkTools
{
    internal class RemoteServerPingStats : IReadOnlyRemoteServerPingStats
    {
        public string Domain { get; set; }

        public bool IsAvailable { get; set; }

        public long Ping { get; set; }

        public IPAddress IP { get; set; }
    }
}
