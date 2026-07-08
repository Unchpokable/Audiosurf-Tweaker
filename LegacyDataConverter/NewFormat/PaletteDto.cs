using System.Collections.Generic;

namespace LegacyDataConverter.NewFormat
{
    /// <summary>
    /// Mirrors the JSON shape produced by SkinChangerRestyle.MVVM.Model.PaletteDynamicLoadContainer/
    /// ColorPalettePrint. Kept as an independent copy on purpose, see SkinManifest.
    /// </summary>
    internal class PaletteContainerDto
    {
        public List<PaletteDto> ColorPalettes { get; set; } = new List<PaletteDto>();
    }

    internal class PaletteDto
    {
        public SerializableColorDto Purple { get; set; }
        public SerializableColorDto Blue { get; set; }
        public SerializableColorDto Green { get; set; }
        public SerializableColorDto Yellow { get; set; }
        public SerializableColorDto Red { get; set; }
        public string Name { get; set; }
    }

    internal class SerializableColorDto
    {
        public byte A { get; set; }
        public byte R { get; set; }
        public byte G { get; set; }
        public byte B { get; set; }

        public float ScA { get; set; }
        public float ScR { get; set; }
        public float ScG { get; set; }
        public float ScB { get; set; }
    }
}
