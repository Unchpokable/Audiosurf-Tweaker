using System;
using Avalonia.Media;

namespace TweakerUI.Core
{
    // HSV/HSL/RGB conversions for the Color Configurator's picker. Ported 1:1 from the arithmetic in
    // the "Palette Designer" mockup (hsvToHex/hexToHsv/hsvToHsl/hslToHsv) so the on-screen picker
    // behaves exactly like the reference design, not a reinvented approximation.
    internal readonly struct Hsv
    {
        public Hsv(double h, double s, double v)
        {
            H = h;
            S = s;
            V = v;
        }

        public double H { get; }
        public double S { get; }
        public double V { get; }
    }

    internal readonly struct Hsl
    {
        public Hsl(double h, double s, double l)
        {
            H = h;
            S = s;
            L = l;
        }

        public double H { get; }
        public double S { get; }
        public double L { get; }
    }

    internal static class ColorMath
    {
        public static Color HsvToColor(double h, double s, double v)
        {
            s /= 100.0;
            v /= 100.0;
            var c = v * s;
            var x = c * (1 - Math.Abs((h / 60.0) % 2 - 1));
            var m = v - c;
            double r = 0, g = 0, b = 0;

            if (h < 60) { r = c; g = x; b = 0; }
            else if (h < 120) { r = x; g = c; b = 0; }
            else if (h < 180) { r = 0; g = c; b = x; }
            else if (h < 240) { r = 0; g = x; b = c; }
            else if (h < 300) { r = x; g = 0; b = c; }
            else { r = c; g = 0; b = x; }

            return Color.FromRgb(ToByte(r + m), ToByte(g + m), ToByte(b + m));
        }

        public static Hsv ColorToHsv(Color color)
        {
            var r = color.R / 255.0;
            var g = color.G / 255.0;
            var b = color.B / 255.0;

            var max = Math.Max(r, Math.Max(g, b));
            var min = Math.Min(r, Math.Min(g, b));
            var d = max - min;

            double h = 0;
            if (d != 0)
            {
                if (max == r) h = 60 * (((g - b) / d) % 6);
                else if (max == g) h = 60 * ((b - r) / d + 2);
                else h = 60 * ((r - g) / d + 4);
            }
            if (h < 0) h += 360;

            var s = max == 0 ? 0 : d / max * 100;
            var v = max * 100;
            return new Hsv(h, s, v);
        }

        public static Hsl HsvToHsl(double h, double s, double v)
        {
            s /= 100.0;
            v /= 100.0;
            var l = v * (1 - s / 2);
            var sl = l == 0 || l == 1 ? 0 : (v - l) / Math.Min(l, 1 - l);
            return new Hsl(h, sl * 100, l * 100);
        }

        public static Hsv HslToHsv(double h, double s, double l)
        {
            s /= 100.0;
            l /= 100.0;
            var v = l + s * Math.Min(l, 1 - l);
            var sv = v == 0 ? 0 : 2 * (1 - l / v) * 100;
            return new Hsv(h, sv, v * 100);
        }

        public static string ToHex(Color color) => $"#{color.R:X2}{color.G:X2}{color.B:X2}";

        public static bool TryParseHex(string hex, out Color color)
        {
            color = default;
            if (string.IsNullOrEmpty(hex)) return false;
            hex = hex.TrimStart('#');
            if (hex.Length != 6) return false;
            if (!byte.TryParse(hex.Substring(0, 2), System.Globalization.NumberStyles.HexNumber, null, out var r)) return false;
            if (!byte.TryParse(hex.Substring(2, 2), System.Globalization.NumberStyles.HexNumber, null, out var g)) return false;
            if (!byte.TryParse(hex.Substring(4, 2), System.Globalization.NumberStyles.HexNumber, null, out var b)) return false;
            color = Color.FromRgb(r, g, b);
            return true;
        }

        private static byte ToByte(double channel01) => (byte)Math.Round(Math.Clamp(channel01, 0, 1) * 255);
    }
}
