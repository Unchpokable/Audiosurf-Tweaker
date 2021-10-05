namespace ChangerAPI
{
    using System.Collections.Generic;
    using ChangerAPI.Utilities;

    public static class EnvironmentalVeriables
    {
        #region Editable Variables
        public static string TempSkinName { get; set; }
        #endregion

        #region Readonly Variables
        //internal static string gamePath = "";
        //internal static string skinsFolderPath = "None";
        public static readonly string CliffImagesMask = "cliff";
        public static readonly string HitImageMask = "hit";
        public static readonly string ParticlesImageMask = "particles";
        public static readonly string RingsImageMask = "ring";
        public static readonly string SkysphereImagesMask = "skysphere";
        public static readonly string TileFlyupImageMask = "tileflyup.png";
        public static readonly string TilesImageNameMask = "tiles.png";
        public static readonly bool DCSWarningsAllowed = true;
        public static readonly string LegacySkinExtention = ".askin";
        public static readonly string ActualSkinExtention = ".askin2";
        #endregion
    }
}
