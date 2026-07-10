using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using Avalonia.Media;
using TweakerUI.Models;

namespace TweakerUI.Core
{
    internal class AudiosurfConfigurationPresenter
    {
        private readonly Dictionary<string, float> _configurations = new Dictionary<string, float>();
        private readonly NumberFormatInfo _floatFormatInfo = new NumberFormatInfo() { NumberDecimalSeparator = "." };
        private string _file;

        public float this[string section]
        {
            get
            {
                if (_configurations.ContainsKey(section))
                    return _configurations[section];
                throw new ArgumentException($"No such section: {section}");
            }

            set
            {
                if (!_configurations.ContainsKey(section))
                    throw new ArgumentException($"No such section: {section}");
                _configurations[section] = value;
            }
        }

        // The game's options.ini stores colors as scRGB (gamma-expanded, linear-light) floats, which
        // is what WPF's Color.ScR/ScG/ScB exposed automatically. Avalonia.Media.Color only carries
        // plain sRGB bytes, so the sRGB<->linear conversion WPF did internally has to be reimplemented
        // here to keep writing/reading the same file format the game and old Tweaker versions expect.
        private static float SRgbToLinear(byte channel)
        {
            var v = channel / 255f;
            return v <= 0.04045f ? v / 12.92f : MathF.Pow((v + 0.055f) / 1.055f, 2.4f);
        }

        private static byte LinearToSRgb(float value)
        {
            if (value <= 0f) return 0;
            if (value >= 1f) return 255;
            var v = value <= 0.0031308f ? value * 12.92f : 1.055f * MathF.Pow(value, 1f / 2.4f) - 0.055f;
            return (byte)Math.Clamp(MathF.Round(v * 255f), 0f, 255f);
        }

        public void SetColor(ASColors targetColor, Color rgb)
        {
            var r = SRgbToLinear(rgb.R);
            var g = SRgbToLinear(rgb.G);
            var b = SRgbToLinear(rgb.B);

            switch (targetColor)
            {
                case ASColors.Purple:
                    _configurations["color4_r"] = r;
                    _configurations["color4_g"] = g;
                    _configurations["color4_b"] = b;
                    return;
                case ASColors.Blue:
                    _configurations["color3_r"] = r;
                    _configurations["color3_g"] = g;
                    _configurations["color3_b"] = b;
                    return;
                case ASColors.Green:
                    _configurations["color2_r"] = r;
                    _configurations["color2_g"] = g;
                    _configurations["color2_b"] = b;
                    return;
                case ASColors.Yellow:
                    _configurations["color1_r"] = r;
                    _configurations["color1_g"] = g;
                    _configurations["color1_b"] = b;
                    return;
                case ASColors.Red:
                    _configurations["color0_r"] = r;
                    _configurations["color0_g"] = g;
                    _configurations["color0_b"] = b;
                    return;
            }
        }

        public Color GetColor(ASColors targetColor)
        {
            (string r, string g, string b) keys = targetColor switch
            {
                ASColors.Purple => ("color4_r", "color4_g", "color4_b"),
                ASColors.Blue => ("color3_r", "color3_g", "color3_b"),
                ASColors.Green => ("color2_r", "color2_g", "color2_b"),
                ASColors.Yellow => ("color1_r", "color1_g", "color1_b"),
                ASColors.Red => ("color0_r", "color0_g", "color0_b"),
                _ => (null, null, null),
            };

            if (keys.r == null)
                return Colors.White;

            return Color.FromRgb(
                LinearToSRgb(_configurations[keys.r]),
                LinearToSRgb(_configurations[keys.g]),
                LinearToSRgb(_configurations[keys.b]));
        }

        public void ProcessConfigFile(string path)
        {
            if (!File.Exists(path))
                throw new ArgumentException($"File does not exists: {path}");
            _file = path;
            using (var file = new StreamReader(path))
            {
                while (!file.EndOfStream)
                {
                    var rawLine = file.ReadLine();
                    if (string.IsNullOrEmpty(rawLine))
                        continue;
                    var line = rawLine.Split(':').Select(x => x.Trim()).ToArray();
                    _configurations[line[0]] = float.Parse(line[1], CultureInfo.InvariantCulture);
                }
            }
        }

        public void ApplyPalette(ColorPalette palette)
        {
            SetColor(ASColors.Purple, palette.Purple);
            SetColor(ASColors.Blue, palette.Blue);
            SetColor(ASColors.Green, palette.Green);
            SetColor(ASColors.Yellow, palette.Yellow);
            SetColor(ASColors.Red, palette.Red);
        }

        public void SaveChanges()
        {
            using (var file = new StreamWriter(_file, false))
            {
                foreach (var record in _configurations.Keys)
                {
                    file.WriteLine($"{record}: {_configurations[record].ToString(_floatFormatInfo)}");
                }
            }
        }

        public ColorPalette ExportPalette()
        {
            return new ColorPalette
            {
                Purple = GetColor(ASColors.Purple),
                Blue = GetColor(ASColors.Blue),
                Green = GetColor(ASColors.Green),
                Yellow = GetColor(ASColors.Yellow),
                Red = GetColor(ASColors.Red)
            };
        }
    }
}
