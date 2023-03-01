using SkinChangerRestyle.Core;
using System;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Formatters.Binary;
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
                OnPropertyChanged(nameof(Name));
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
            return string.Equals(this.Name, other.Name)
                && Purple == other.Purple
                && Blue == other.Blue
                && Green == other.Green
                && Yellow == other.Yellow
                && Red == other.Red;
        }

        public static bool Save(ColorPalette obj, string path)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                using (var file = new FileStream($"{path}\\{obj.Name}{ColorPalettePrint.PaletteFileExtension}", FileMode.Create))
                {
                    var print = new ColorPalettePrint(obj);
                    formatter.Serialize(file, print);
                    return true;
                }
            }
            catch
            {
                return false;
            }
        }

        public static ColorPalette Load(string path)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                using(var file = new FileStream(path, FileMode.Open))
                {
                    var obj = (ColorPalettePrint)formatter.Deserialize(file);
                    return new ColorPalette(obj);
                }
            }
            catch
            {
                return null;
            }
        }
    }
}
