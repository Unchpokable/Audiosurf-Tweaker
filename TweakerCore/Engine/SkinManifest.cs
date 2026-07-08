using System.Collections.Generic;

namespace TweakerCore.Engine
{
    internal class SkinManifest
    {
        public int FormatVersion { get; set; } = 1;
        public string Name { get; set; }
        public string Uid { get; set; }
        public List<string> Cliffs { get; set; } = new List<string>();
        public List<string> Hits { get; set; } = new List<string>();
        public List<string> Particles { get; set; } = new List<string>();
        public List<string> Rings { get; set; } = new List<string>();
        public List<string> SkySpheres { get; set; } = new List<string>();
        public List<string> SkySphereSource { get; set; } = new List<string>();
        public List<string> Previews { get; set; } = new List<string>();
        public string Tiles { get; set; }
        public string TilesFlyup { get; set; }
        public string Cover { get; set; }
    }
}
