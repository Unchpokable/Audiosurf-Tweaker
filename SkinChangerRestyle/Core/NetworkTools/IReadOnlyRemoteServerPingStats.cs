using System;
using System.Net;

namespace SkinChangerRestyle.Core.NetworkTools
{
    internal interface IReadOnlyRemoteServerPingStats
    {
        string Domain { get; }
        bool IsAvailable { get; }
        long Ping { get; }
        IPAddress IP { get; }
    }
}
