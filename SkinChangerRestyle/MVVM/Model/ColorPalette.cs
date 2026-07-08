using ChangerAPI.Engine;
using SkinChangerRestyle.Core;
using System;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Windows.Media;


namespace SkinChangerRestyle.MVVM.Model
{
    internal class ColorPalette : ObservableObject, IEquatable<ColorPalette>
    {
        /// <summary>
        /// Creates a new <see cref="ColorPalette"/> object with default colors
        /// </summary>
        public ColorPalette()
        {

        }

        /// <summary>
        /// Creates a new <see cref="ColorPalette"/> object based on given <see cref="ColorPalette"/> object
        /// </summary>
        /// <param name="origin">Origin Color Palette</param>
        public ColorPalette(ColorPalette origin)
        {
            Name = origin.Name;
            Purple = origin.Purple;
            Blue = origin.Blue;
            Green = origin.Green;
            Yellow = origin.Yellow;
            Red = origin.Red;
        }

        /// <summary>
        /// Creates a new <see cref="ColorPalette"/> object based on given <see cref="ColorPalettePrint"/> object
        /// </summary>
        /// <param name="print"></param>
        public ColorPalette(ColorPalettePrint print)
        {
            Name = print.Name;
            Purple = print.Purple;
            Red = print.Red;
            Yellow = print.Yellow;
            Blue = print.Blue;
            Green = print.Green;
        }

        public string Name
        {
            get => _name;
            set
            {
                _name = value;
                OnPropertyChanged();
            }
        }

        public Color Purple
        {
            get => _purple;
            set
            {
                _purple = value;
                OnPropertyChanged();
            }
        }

        public Color Blue
        {
            get => _blue;
            set
            {
                _blue = value;
                OnPropertyChanged();
            }
        }

        public Color Green
        {
            get => _green;
            set
            {
                _green = value;
                OnPropertyChanged();
            }
        }

        public Color Yellow
        {
            get => _yellow; 
            set
            {
                _yellow = value;
                OnPropertyChanged();
            }
        }

        public Color Red
        {
            get => _red; 
            set
            {
                _red = value;
                OnPropertyChanged();
            }
        }

        private string _name;
        private Color _purple;
        private Color _blue;
        private Color _green;
        private Color _yellow;
        private Color _red;

        public bool Equals(ColorPalette other)
        {
            return other != null
                   && string.Equals(this.Name, other.Name)
                   && Purple == other.Purple
                   && Blue == other.Blue
                   && Green == other.Green
                   && Yellow == other.Yellow
                   && Red == other.Red;
        }

        private static readonly Logger _logger = new Logger();
        private static readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions { WriteIndented = true };

        public static bool Save(ColorPalette obj, string path)
        {
            var fullPath = $"{path}\\{obj.Name}{ColorPalettePrint.PaletteFileExtension}";
            try
            {
                var print = new ColorPalettePrint(obj);
                File.WriteAllText(fullPath, JsonSerializer.Serialize(print, _jsonOptions), Encoding.UTF8);
                return true;
            }
            catch (Exception ex)
            {
                _logger.Log("ColorPalette", $"Failed to save palette to '{fullPath}': {ex}");
                return false;
            }
        }

        public static ColorPalette Load(string path)
        {
            try
            {
                if (!LooksLikeJson(path) && !LegacyConverter.TryConvert(path))
                    return null;

                var json = File.ReadAllText(path, Encoding.UTF8);
                var obj = JsonSerializer.Deserialize<ColorPalettePrint>(json);
                return new ColorPalette(obj);
            }
            catch (Exception ex)
            {
                _logger.Log("ColorPalette", $"Failed to load palette from '{path}': {ex}");
                return null;
            }
        }

        private static bool LooksLikeJson(string path)
        {
            using (var reader = new StreamReader(path))
            {
                int ch;
                while ((ch = reader.Read()) != -1)
                {
                    if (char.IsWhiteSpace((char)ch))
                        continue;
                    return ch == '{';
                }
                return false;
            }
        }
    }
}
