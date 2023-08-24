using System;
using System.Windows.Media;

namespace SkinChangerRestyle.MVVM.Model
{
    [Serializable]
    internal class ColorPalettePrint : IEquatable<ColorPalettePrint>
    {
        public ColorPalettePrint(ColorPalette origin)
        {
            Purple = new SerializableColor(origin.Purple);
            Red = new SerializableColor(origin.Red);
            Green = new SerializableColor(origin.Green);
            Blue = new SerializableColor(origin.Blue);
            Yellow = new SerializableColor(origin.Yellow);
            Name = origin.Name;
        }

        public static string PaletteFileExtension = ".palette";

        public SerializableColor Purple { get; set; }
        public SerializableColor Blue { get; set; }
        public SerializableColor Green { get; set; }
        public SerializableColor Yellow { get; set; }
        public SerializableColor Red { get; set; }
        public string Name { get; set; }

        public bool Equals(ColorPalettePrint other)
        {
            return other != null
                   && string.Equals(this.Name, other.Name)
                   && Purple == other.Purple
                   && Blue == other.Blue
                   && Green == other.Green
                   && Yellow == other.Yellow
                   && Red == other.Red;
        }

        [Serializable]
        public struct SerializableColor
        {
            public SerializableColor(Color origin)
            {
                A = origin.A;
                R = origin.R;
                G = origin.G;
                B = origin.B;

                ScA = origin.ScA;
                ScR = origin.ScR;
                ScG = origin.ScG;
                ScB = origin.ScB;
            }

            public byte A { get; set; }
            public byte R { get; set; }
            public byte G { get; set; }
            public byte B { get; set; }

            public float ScA { get; set; }
            public float ScR { get; set; }
            public float ScG { get; set; }
            public float ScB { get; set; }

            public static bool operator==(SerializableColor left, SerializableColor right)
            {
                return left.A == right.A
                    && left.R == right.R
                    && left.G == right.G
                    && left.B == right.B
                    && left.ScA == right.ScA
                    && left.ScR == right.ScR
                    && left.ScG == right.ScG
                    && left.ScB == right.ScB;
            }

            public static bool operator!=(SerializableColor left, SerializableColor right)
            {
                return left.A != right.A
                    || left.R != right.R
                    || left.G != right.G
                    || left.B != right.B
                    || left.ScA != right.ScA
                    || left.ScR != right.ScR
                    || left.ScG != right.ScG
                    || left.ScB != right.ScB;
            }

            public static implicit operator Color(SerializableColor value)
            {
                return new Color() { A = value.A, R = value.R, G = value.G, B = value.B, ScA = value.ScA, ScR = value.ScR, ScG = value.ScG, ScB = value.ScB };
            }

            public override bool Equals(object obj)
            {
                return obj is SerializableColor color &&
                       A == color.A &&
                       R == color.R &&
                       G == color.G &&
                       B == color.B &&
                       ScA == color.ScA &&
                       ScR == color.ScR &&
                       ScG == color.ScG &&
                       ScB == color.ScB;
            }

            public override int GetHashCode()
            {
                int hashCode = 737535892;
                hashCode = hashCode * -1521134295 + A.GetHashCode();
                hashCode = hashCode * -1521134295 + R.GetHashCode();
                hashCode = hashCode * -1521134295 + G.GetHashCode();
                hashCode = hashCode * -1521134295 + B.GetHashCode();
                hashCode = hashCode * -1521134295 + ScA.GetHashCode();
                hashCode = hashCode * -1521134295 + ScR.GetHashCode();
                hashCode = hashCode * -1521134295 + ScG.GetHashCode();
                hashCode = hashCode * -1521134295 + ScB.GetHashCode();
                return hashCode;
            }
        }
    }
}
