using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SkinChangerRestyle.MVVM.ViewModel
{
    internal class TweakerViewModel
    {
        public TweakerViewModel()
        {

        }

        private Dictionary<string, string> addFeaturesChecksEnableCommandRoute = new Dictionary<string, string>()
        {
            {"hideroad", "asconfig roadvisible false" },
            {"sidewinder", "asconfig sidewinder true" },
            {"bankcam", "asconfig usebankingcamera true" },
            {"hidesong", "asconfig showsongname false" }
        };

        private Dictionary<string, string> addFeaturesChecksDisableCommandRoute = new Dictionary<string, string>()
        {
            {"hideroad", "asconfig roadvisible true" },
            {"sidewinder", "asconfig sidewinder false" },
            {"bankcam", "asconfig usebankingcamera false" },
            {"hidesong", "asconfig showsongname true" }
        };
    }
}
