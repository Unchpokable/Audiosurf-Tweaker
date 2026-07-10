using System;
using Avalonia.Media;

namespace TweakerUI.Models
{
    internal class ColorPalettePrint : IEquatable<ColorPalettePrint>
    {
        public ColorPalettePrint()
        {
        }

        public ColorPalettePrint(ColorPalette origin)
        {
            Id = origin.Id;
            Purple = new SerializableColor(origin.Purple);
            Red = new SerializableColor(origin.Red);
            Green = new SerializableColor(origin.Green);
            Blue = new SerializableColor(origin.Blue);
            Yellow = new SerializableColor(origin.Yellow);
            Name = origin.Name;
        }

        public static string PaletteFileExtension = ".palette";

        public Guid Id { get; set; }
        public SerializableColor Purple { get; set; }
        public SerializableColor Blue { get; set; }
        public SerializableColor Green { get; set; }
        public SerializableColor Yellow { get; set; }
        public SerializableColor Red { get; set; }
        public string Name { get; set; }

        public bool Equals(ColorPalettePrint other)
        {
            return other != null
                   && string.Equals(Name, other.Name)
                   && Purple == other.Purple
                   && Blue == other.Blue
                   && Green == other.Green
                   && Yellow == other.Yellow
                   && Red == other.Red;
        }

        // Only plain sRGB bytes are kept - the WPF original also carried scRGB (linear) float
        // components, but those were never read back from the JSON, only recomputed by WPF's own
        // Color struct at access time. Avalonia.Media.Color has no scRGB representation, and the
        // game-file linear-space math in AudiosurfConfigurationPresenter derives it directly from
        // these bytes, so keeping the extra fields here would be dead weight.
        public struct SerializableColor
        {
            public SerializableColor(Color origin)
            {
                A = origin.A;
                R = origin.R;
                G = origin.G;
                B = origin.B;
            }

            public byte A { get; set; }
            public byte R { get; set; }
            public byte G { get; set; }
            public byte B { get; set; }

            public static bool operator ==(SerializableColor left, SerializableColor right)
            {
                return left.A == right.A && left.R == right.R && left.G == right.G && left.B == right.B;
            }

            public static bool operator !=(SerializableColor left, SerializableColor right)
            {
                return !(left == right);
            }

            public static implicit operator Color(SerializableColor value)
            {
                return Color.FromArgb(value.A, value.R, value.G, value.B);
            }

            public override bool Equals(object obj)
            {
                return obj is SerializableColor color && this == color;
            }

            public override int GetHashCode()
            {
                return HashCode.Combine(A, R, G, B);
            }
        }
    }
}
