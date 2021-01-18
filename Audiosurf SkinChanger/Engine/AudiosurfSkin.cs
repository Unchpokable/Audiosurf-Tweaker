namespace Audiosurf_SkinChanger.Engine
{
    using System;
    using System.Drawing;

    [Serializable]
    class AudiosurfSkin
    {
        public string Source { get; set; }
        public string Name { get; set; }
        public Bitmap SkySphere { get; set; }
        public Bitmap SkySphereSource { get; set; }
        public Bitmap[] Tiles { get; set; }
        public Bitmap[] Particles { get; set; }
        public Bitmap[] Rings { get; set; }
    }
}
