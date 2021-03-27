namespace Audiosurf_SkinChanger.Skin_Creator
{
    using System;

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
