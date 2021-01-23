namespace Audiosurf_SkinChanger
{
    using Audiosurf_SkinChanger.Engine;
    using System.Collections.Generic;

    public static class EnvironmentalVeriables
    {
        internal static readonly IList<AudiosurfSkin> Skins = new List<AudiosurfSkin>();
        internal static string gamePath = "";
        internal static string skinsFolderPath = "";
        internal static readonly string CliffImagesMask = "cliff";
        internal static readonly string HitImageMask = "hit";
        internal static readonly string ParticlesImageMask = "particles";
        internal static readonly string RingsImageMask = "ring";
        internal static readonly string SkysphereImagesMask = "skysphere";
        internal static readonly string TileFlyupImageMask = "tileflyup.png";
        internal static readonly string TilesImageName = "tiles.png";
    }
}
