namespace Audiosurf_SkinChanger.Engine
{
    using System;
    using System.Drawing;
    using Audiosurf_SkinChanger.Utilities;

    [Serializable]
    class AudiosurfSkin
    {
        public string Source { get; set; }
        public string Name { get; set; }
        public Bitmap SkySphere { get; set; }
        public Bitmap SkySphereSource { get; set; }
        public ImageGroup Tiles { get; set; }
        public ImageGroup Particles { get; set; }
        public ImageGroup Rings { get; set; }
    }
}
