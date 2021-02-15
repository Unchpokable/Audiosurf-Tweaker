namespace Audiosurf_SkinChanger.Engine
{
    using System;
    using System.Drawing;
    using Audiosurf_SkinChanger.Utilities;
    using Audiosurf_SkinChanger.Engine;

    [Serializable]
    class AudiosurfSkin
    {
        public string Source { get; set; }
        public string Name { get; set; }
        public ImageGroup SkySpheres { get; set; }
        public ImageGroup SkySphereSource { get; set; }
        public ImageGroup Cliffs { get; set; }
        public ImageGroup Hits { get; set; }
        public NamedBitmap Tiles { get; set; }
        public NamedBitmap TilesFlyup { get; set; }
        public ImageGroup Particles { get; set; }
        public ImageGroup Rings { get; set; }

        public AudiosurfSkin()
        {
            SkySpheres = new ImageGroup();
            Cliffs = new ImageGroup();
            Hits = new ImageGroup();
            Particles = new ImageGroup();
            Rings = new ImageGroup();
        }


        public override string ToString()
        {
            return Name;
        }
    }
}
