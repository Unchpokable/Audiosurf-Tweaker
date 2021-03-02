using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Audiosurf_SkinChanger.Skin_Creator
{
    [Serializable]
    public class ImageInfo
    {
        public string Format { get; set; }
        public string FileName { get; set; }
        public string PathToFile { get; set; }

        public ImageInfo(string format, string fileName)
        {
            Format = format;
            FileName = fileName;
        }
    }
}
