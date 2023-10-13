using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using DiscordRPC;
using DiscordRichPresence;
using DiscordRPC.Message;

namespace DiscordRichPresence
{
    public class RichPresenceClient
    {
        public RichPresenceClient()
        {
            _appId = _defaultAppId;
        }

        public RichPresenceClient(int discordPipe) : this()
        {
            _pipe = discordPipe;
        }



        private DiscordRPC.Logging.LogLevel _logLevel = DiscordRPC.Logging.LogLevel.Trace;
        private int _pipe = -1;

        private string _defaultAppId = "1041109722583011328";
        private string _appId;

        private RichPresenceClient _client;

        public void UpdateApplicationId(string appId)
        {
            if (!appId.All(char.IsDigit))
                throw new ArgumentException("Discord Application Id should be a 21-number string value");
            _appId = appId;
        }

        public void UpdatePresence(string state, IEnumerable<string> details, Assets customAssets = null)
        {

        }
    }
}