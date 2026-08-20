using System;
using System.IO;
using System.Text;
using System.Text.Json;
using Avalonia.Media;
using CommunityToolkit.Mvvm.ComponentModel;
using TweakerCore.Engine;
using TweakerUI.Core;

namespace TweakerUI.Models
{
    public partial class ColorPalette : ObservableObject, IEquatable<ColorPalette>
    {
        public ColorPalette()
        {
        }

        public ColorPalette(ColorPalette origin)
        {
            Id = origin.Id;
            Name = origin.Name;
            Purple = origin.Purple;
            Blue = origin.Blue;
            Green = origin.Green;
            Yellow = origin.Yellow;
            Red = origin.Red;
        }

        internal ColorPalette(ColorPalettePrint print)
        {
            // Guid.Empty means this print predates the Id field (or came from a legacy-converted
            // file) - treat it as a fresh identity rather than letting every such entry collide.
            Id = print.Id == Guid.Empty ? Guid.NewGuid() : print.Id;
            Name = print.Name;
            Purple = print.Purple;
            Blue = print.Blue;
            Green = print.Green;
            Yellow = print.Yellow;
            Red = print.Red;
        }

        // Stable identity independent of Name/colors - storage lookups (rename, replace, remove)
        // key off this instead of value-equality, so two palettes that happen to share a name and
        // colors (e.g. two untouched "New Palette" defaults) can never be confused for each other.
        public Guid Id { get; set; } = Guid.NewGuid();

        [ObservableProperty]
        private string name;

        [ObservableProperty]
        private Color purple;

        [ObservableProperty]
        private Color blue;

        [ObservableProperty]
        private Color green;

        [ObservableProperty]
        private Color yellow;

        [ObservableProperty]
        private Color red;

        // Transient UI state, not persisted (ColorPalettePrint doesn't carry it) - drives the
        // palette list row highlight in the Color Configurator.
        [ObservableProperty]
        private bool isSelected;

        public IBrush PurpleBrush => new SolidColorBrush(Purple);
        public IBrush BlueBrush => new SolidColorBrush(Blue);
        public IBrush GreenBrush => new SolidColorBrush(Green);
        public IBrush YellowBrush => new SolidColorBrush(Yellow);
        public IBrush RedBrush => new SolidColorBrush(Red);

        partial void OnPurpleChanged(Color value) => OnPropertyChanged(nameof(PurpleBrush));
        partial void OnBlueChanged(Color value) => OnPropertyChanged(nameof(BlueBrush));
        partial void OnGreenChanged(Color value) => OnPropertyChanged(nameof(GreenBrush));
        partial void OnYellowChanged(Color value) => OnPropertyChanged(nameof(YellowBrush));
        partial void OnRedChanged(Color value) => OnPropertyChanged(nameof(RedBrush));

        public bool Equals(ColorPalette other) => other != null && Id == other.Id;
        public override bool Equals(object obj) => obj is ColorPalette other && Equals(other);
        public override int GetHashCode() => Id.GetHashCode();

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
                Logger.Log("ColorPalette", $"Failed to save palette to '{fullPath}': {ex}");
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
                Logger.Log("ColorPalette", $"Failed to load palette from '{path}': {ex}");
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
