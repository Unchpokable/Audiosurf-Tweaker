using System.Net;

namespace TweakerUI.Core.NetworkTools
{
    internal interface IReadOnlyRemoteServerPingStats
    {
        string Domain { get; }
        bool IsAvailable { get; }
        long Ping { get; }
        IPAddress IP { get; }
    }
}
