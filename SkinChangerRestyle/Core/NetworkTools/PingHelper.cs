using SkinChangerRestyle.Core.NetworkTools.Exceptions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.NetworkInformation;
using System.Net;
using System.Text;
using System.Threading.Tasks;

namespace SkinChangerRestyle.Core.NetworkTools
{
    internal class PingHelper
    {
        private PingHelper() { }
        
        private int _defaultTimeout = 120;

        public static PingHelper Instance
        {
            get
            {
                if (_instance == null)
                    _instance = new PingHelper();
                return _instance;
            }
        }

        private static PingHelper _instance;

        public async Task<IReadOnlyRemoteServerPingStats> PingHostAsync(string hostName)
        {
            return await PingHostAsync(await GetIPAddressOfHost(hostName));
        }

        public async Task<IReadOnlyRemoteServerPingStats> PingHostAsync(IPAddress address)
        {
            return await Task.Run(() =>
            {
                var data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; // 32 bytes of scrap data
                var buffer = Encoding.ASCII.GetBytes(data);
                var pingSender = new Ping();
                var opt = new PingOptions();
                opt.DontFragment = true;
                var reply = pingSender.Send(address, _defaultTimeout, buffer, opt);
                if (reply == null)
                    throw new Exception("Unknown exception: Ping returned null reply");

                if (reply.Status == IPStatus.Success)
                {
                    var pingStat = new RemoteServerPingStats();
                    pingStat.IsAvailable = true;
                    pingStat.Domain = Dns.GetHostEntry(address).HostName;
                    pingStat.Ping = reply.RoundtripTime;
                    pingStat.IP = address;
                    return pingStat;
                }

                return null;
            });
        }

        private async Task<IPAddress> GetIPAddressOfHost(string host)
        {
            var hostIP = await Dns.GetHostAddressesAsync(host);
            if (hostIP.Length == 0)
                throw new UnresolvedHostnameException($"DNS Server can not resolve given host {host}");

            return hostIP.First();
        }
    }
}
