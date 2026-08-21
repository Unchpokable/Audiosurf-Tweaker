using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;
using TweakerCore.Engine;
using TweakerUI.Core;

namespace TweakerUI.Models
{
    internal class PaletteDynamicLoadContainer
    {
        public PaletteDynamicLoadContainer()
        {
            ColorPalettes = new List<ColorPalettePrint>();
        }

        public static string PaletteContainerFileExtension = ".pltc";

        public List<ColorPalettePrint> ColorPalettes { get; set; }

        private static readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions { WriteIndented = true };

        // Matches by Id, not by value - two untouched "New Palette" defaults have identical
        // name+colors, and matching on value would silently edit whichever one happens to come
        // first in storage instead of the one actually selected in the UI.
        public void ReplaceOrAdd(ColorPalette origin, ColorPalette newPalette)
        {
            var newPrint = new ColorPalettePrint(newPalette);

            for (var i = 0; i < ColorPalettes.Count; i++)
            {
                if (ColorPalettes[i].Id == origin.Id)
                {
                    ColorPalettes[i] = newPrint;
                    return;
                }
            }
            ColorPalettes.Add(newPrint);
        }

        public void Add(ColorPalette origin)
        {
            ColorPalettes.Add(new ColorPalettePrint(origin));
        }

        public void Remove(ColorPalette origin)
        {
            ColorPalettes.RemoveAll(p => p.Id == origin.Id);
        }

        public static PaletteDynamicLoadContainer Load(string filename)
        {
            var path = filename + PaletteContainerFileExtension;
            if (!File.Exists(path))
                return null;

            try
            {
                if (!LooksLikeJson(path) && !LegacyConverter.TryConvert(path))
                    return null;

                var json = File.ReadAllText(path, Encoding.UTF8);
                return JsonSerializer.Deserialize<PaletteDynamicLoadContainer>(json);
            }
            catch (Exception ex)
            {
                Logger.Log("PaletteDynamicLoadContainer", $"Failed to load palette storage from '{path}': {ex}");
                return null;
            }
        }

        public static bool Save(PaletteDynamicLoadContainer obj, string path)
        {
            var fullPath = path + PaletteContainerFileExtension;
            try
            {
                File.WriteAllText(fullPath, JsonSerializer.Serialize(obj, _jsonOptions), Encoding.UTF8);
                return true;
            }
            catch (Exception ex)
            {
                Logger.Log("PaletteDynamicLoadContainer", $"Failed to save palette storage to '{fullPath}': {ex}");
                return false;
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

        public static bool Add(ColorPalette palette, string containerName)
        {
            var containerInstance = Load(containerName);
            if (containerInstance == null)
                return false;

            containerInstance.ColorPalettes.Add(new ColorPalettePrint(palette));
            return Save(containerInstance, containerName);
        }

        public static bool Replace(ColorPalette oldPalette, ColorPalette newPalette, string containerName)
        {
            var containerInstance = Load(containerName);

            if (containerInstance == null)
                return false;

            containerInstance.ReplaceOrAdd(oldPalette, newPalette);
            return Save(containerInstance, containerName);
        }

        public static bool Remove(ColorPalette target, string containerName)
        {
            var containerInstance = Load(containerName);

            if (containerInstance == null)
                return false;

            containerInstance.Remove(target);
            return Save(containerInstance, containerName);
        }
    }
}
