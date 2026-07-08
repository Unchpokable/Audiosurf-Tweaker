using System;
using System.Collections.Generic;

namespace SkinChangerRestyle.MVVM.Model
{
    /// <summary>
    /// Frozen field-layout copy of SkinChangerRestyle.MVVM.Model.PaletteDynamicLoadContainer as it
    /// existed while color presets were still BinaryFormatter-serialized. Do not "clean up" or evolve
    /// this - it exists only so old .pltc files remain deserializable. See LegacyBinder.
    /// </summary>
    [Serializable]
    internal class PaletteDynamicLoadContainer
    {
        public List<ColorPalettePrint> ColorPalettes { get; set; }

        public PaletteDynamicLoadContainer()
        {
            ColorPalettes = new List<ColorPalettePrint>();
        }
    }
}
