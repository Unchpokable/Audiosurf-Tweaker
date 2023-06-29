using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using TweakerScripts;

namespace SkinChangerRestyle.Core.ServerSwapper
{
    internal class ServerSwapper
    {
        public ServerSwapper()
        {
            _defines = new Dictionary<string, string>()
            {
                {"%AS%", Directory.GetDirectoryRoot(Directory.GetDirectoryRoot(SettingsProvider.GameTexturesPath)) }, // => AS\engine\textures -> AS\
                {"%BACKUP_PATH%", Assembly.GetExecutingAssembly().Location + "\\Backups"},
            };
        }
         
        private Dictionary<string, string> _defines;
    }
}