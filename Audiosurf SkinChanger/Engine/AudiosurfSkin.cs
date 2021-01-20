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
        public ImageGroup SkySpheres { get; set; }
        public ImageGroup SkySphereSource { get; set; }
        public ImageGroup Cliffs { get; set; }
        public ImageGroup Hits { get; set; }
        public Bitmap Tiles { get; set; }
        public Bitmap TilesFlyup { get; set; }
        public ImageGroup Particles { get; set; }
        public ImageGroup Rings { get; set; }

        public override string ToString()
        {
            return Name;
        }
    }
}
