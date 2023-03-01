using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Formatters.Binary;
using System.Text;
using System.Threading.Tasks;

namespace SkinChangerRestyle.MVVM.Model
{
    [Serializable]
    internal class PaletteDynamicLoadContainer
    {
        public PaletteDynamicLoadContainer()
        {
            ColorPalettes = new List<ColorPalettePrint>();
        }

        public static string PaletteContainerFileExtension = ".pltc";

        public List<ColorPalettePrint> ColorPalettes { get; private set; }

        public void ReplaceOrAdd(ColorPalette origin, ColorPalette newPalette)
        {
            var printOrig = new ColorPalettePrint(origin);
            var newPrint = new ColorPalettePrint(newPalette);

            for (var i = 0; i < ColorPalettes.Count; i++)
            {
                if (ColorPalettes[i].Equals(printOrig))
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
            ColorPalettes.Remove(new ColorPalettePrint(origin));
        }

        public static PaletteDynamicLoadContainer Load(string filename)
        {
            if (!File.Exists(filename + PaletteContainerFileExtension))
                return null;

            try
            {
                IFormatter formatter = new BinaryFormatter();
                using (var file = new FileStream(filename + PaletteContainerFileExtension, FileMode.Open))
                {
                    return (PaletteDynamicLoadContainer)formatter.Deserialize(file);
                }
            }
            catch
            {
                return null;
            }
        }

        public static bool Save(PaletteDynamicLoadContainer obj, string path)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                using (var file = new FileStream(path + PaletteContainerFileExtension, FileMode.Create))
                {
                    formatter.Serialize(file, obj);
                }
                return true;
            }
            catch
            {
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
