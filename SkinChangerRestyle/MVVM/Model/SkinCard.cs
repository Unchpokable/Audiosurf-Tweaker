using ChangerAPI.Engine;
using SkinChangerRestyle.Core.Extensions;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Media;

namespace SkinChangerRestyle.MVVM.Model
{
    class SkinCard
    {
        public ImageSource Cover => ((Bitmap)pinnedSkin.Cover).ToImageSource();
        public string Name { get; set; }

        private AudiosurfSkinExtended pinnedSkin;
    }
}
