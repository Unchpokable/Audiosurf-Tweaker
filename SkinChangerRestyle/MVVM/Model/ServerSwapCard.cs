using SkinChangerRestyle.Core;
using SkinChangerRestyle.Core.NetworkTools;
using System;
using System.IO;

namespace SkinChangerRestyle.MVVM.Model
{
    internal class ServerSwapCard : ObservableObject
    {
        public ServerSwapCard(string baseDirectory)
        {
            BasePackagePath = baseDirectory;
        }

        public string ServerName { get; set; }

        public string ServerHost
        {
            get => RemoteStats.Domain;
            set
            {
                RemoteStats = PingHelper.Instance.PingHostAsync(value).GetAwaiter().GetResult();
                NotifyStatisticsChanged();
            }
        }

        public string ServerPing => RemoteStats.Ping.ToString();
        

        public bool IsAvaliable => RemoteStats.IsAvailable;

        public IReadOnlyRemoteServerPingStats RemoteStats { get; set; }

        public string BasePackagePath { get; private set; }

        public async void ActualizeRemoteStats()
        {
            RemoteStats = await PingHelper.Instance.PingHostAsync(RemoteStats.IP);
            NotifyStatisticsChanged();
        }
        
        private void NotifyStatisticsChanged()
        {
            OnPropertyChanged(nameof(ServerName));
            OnPropertyChanged(nameof(ServerHost));
            OnPropertyChanged(nameof(ServerPing));
            OnPropertyChanged(nameof(IsAvaliable));
        }
    }
}
