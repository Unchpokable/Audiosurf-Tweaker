using SkinChangerRestyle.Core.NetworkTools.Exceptions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Text;
using System.Threading.Tasks;
using TweakerScripts.Exceptions;

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
