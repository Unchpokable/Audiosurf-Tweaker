using SkinChangerRestyle.MVVM.Model;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Media;

namespace SkinChangerRestyle.Core
{
    internal class AudiosurfConfigurationPresenter
    {
        public AudiosurfConfigurationPresenter()
        {
            _configurations = new Dictionary<string, float>();
        }

        private Dictionary<string, float> _configurations;
        private NumberFormatInfo _floatFormatInfo = new NumberFormatInfo() { NumberDecimalSeparator = "." };
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
                if (_configurations.ContainsKey(section))
                    _configurations[section] = value;
                throw new ArgumentException($"No such section: {section}");
            }
        }

        public void SetColor(ASColors targetColor, Color rgb)
        {
            switch (targetColor)
            {
                case ASColors.Purple:
                    _configurations["color4_r"] = rgb.ScR;
                    _configurations["color4_g"] = rgb.ScG;
                    _configurations["color4_b"] = rgb.ScB;
                    return;
                case ASColors.Blue:
                    _configurations["color3_r"] = rgb.ScR;
                    _configurations["color3_g"] = rgb.ScG;
                    _configurations["color3_b"] = rgb.ScB;
                    return;
                case ASColors.Green:
                    _configurations["color2_r"] = rgb.ScR;
                    _configurations["color2_g"] = rgb.ScG;
                    _configurations["color2_b"] = rgb.ScB;
                    return;
                case ASColors.Yellow:
                    _configurations["color1_r"] = rgb.ScR;
                    _configurations["color1_g"] = rgb.ScG;
                    _configurations["color1_b"] = rgb.ScB;
                    return;
                case ASColors.Red:
                    _configurations["color0_r"] = rgb.ScR;
                    _configurations["color0_g"] = rgb.ScG;
                    _configurations["color0_b"] = rgb.ScB;
                    return;
            }
        }


        
        public Color GetColor(ASColors targetColor)
        {
            var rgb = new Color();

            switch (targetColor)
            {
                case ASColors.Purple:
                    rgb = Color.FromScRgb(1f, _configurations["color4_r"], _configurations["color4_g"], _configurations["color4_b"]);
                    break;
                case ASColors.Blue:
                    rgb = Color.FromScRgb(1f, _configurations["color3_r"], _configurations["color3_g"], _configurations["color3_b"]);
                    break;
                case ASColors.Green:
                    rgb = Color.FromScRgb(1f, _configurations["color2_r"], _configurations["color2_g"], _configurations["color2_b"]);
                    break;
                case ASColors.Yellow:
                    rgb = Color.FromScRgb(1f, _configurations["color1_r"], _configurations["color1_g"], _configurations["color1_b"]);
                    break;
                case ASColors.Red:
                    rgb = Color.FromScRgb(1f, _configurations["color0_r"], _configurations["color0_g"], _configurations["color0_b"]);
                    break;
            }
            return rgb;
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
                    var line = file.ReadLine().Split(':').Select(x => x.Trim()).ToArray();
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
            var palette = new ColorPalette();

            palette.Purple = GetColor(ASColors.Purple);
            palette.Blue = GetColor(ASColors.Blue);
            palette.Green = GetColor(ASColors.Green);
            palette.Yellow = GetColor(ASColors.Yellow);
            palette.Red = GetColor(ASColors.Red);

            return palette;
        }
    }
}
