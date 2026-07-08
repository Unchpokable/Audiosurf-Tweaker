using System;

namespace SkinChangerRestyle.MVVM.Model
{
    /// <summary>
    /// Frozen field-layout copy of SkinChangerRestyle.MVVM.Model.ColorPalettePrint as it existed
    /// while color presets were still BinaryFormatter-serialized. Do not "clean up" or evolve this -
    /// it exists only so old .palette/.pltc files remain deserializable. See LegacyBinder.
    /// </summary>
    [Serializable]
    internal class ColorPalettePrint
    {
        public SerializableColor Purple { get; set; }
        public SerializableColor Blue { get; set; }
        public SerializableColor Green { get; set; }
        public SerializableColor Yellow { get; set; }
        public SerializableColor Red { get; set; }
        public string Name { get; set; }

        [Serializable]
        public struct SerializableColor
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
}
