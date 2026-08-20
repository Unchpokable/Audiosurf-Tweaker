using ChangerAPI.Engine;
using LegacyDataConverter.Legacy;
using LegacyDataConverter.NewFormat;
using SkinChangerRestyle.MVVM.Model;
using System;
using System.IO;
using System.Runtime.Serialization.Formatters.Binary;
using System.Text;
using System.Text.Json;

namespace LegacyDataConverter
{
    /// <summary>
    /// Standalone .NET Framework companion tool: converts old BinaryFormatter-based Audiosurf Tweaker
    /// files (.tasp/.askin2 skins, .pltc palette storages, .palette single presets) to their current
    /// formats in place. Invoked by the main (.NET 10+) Tweaker as an external process, since
    /// BinaryFormatter is unavailable there. See Docs/revival-roadmap.md, Phase 2.
    /// </summary>
    internal static class Program
    {
        private static readonly JsonSerializerOptions JsonOptions = new JsonSerializerOptions { WriteIndented = true };

        private static int Main(string[] args)
        {
            if (args.Length < 1)
            {
                Console.WriteLine("Usage: LegacyDataConverter.exe <path-to-file>");
                return 2;
            }

            var path = args[0];
            if (!File.Exists(path))
            {
                Console.WriteLine($"File not found: {path}");
                return 2;
            }

            try
            {
                // Every converter below either completes or throws (a legacy blob that BinaryFormatter
                // cannot read, a file that cannot be rewritten) - failure reaches the caller as exit
                // code 1 through the catch, never as a return value.
                switch (Path.GetExtension(path).ToLowerInvariant())
                {
                    case ".tasp":
                    case ".askin2":
                        ConvertSkin(path);
                        return 0;
                    case ".pltc":
                        ConvertPaletteContainer(path);
                        return 0;
                    case ".palette":
                        ConvertSinglePalette(path);
                        return 0;
                    default:
                        Console.WriteLine($"Unrecognized file type: {path}");
                        return 2;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Conversion failed: {ex}");
                return 1;
            }
        }

        private static void ConvertSkin(string path)
        {
            var skin = Deserialize<AudiosurfSkinExtended>(path);
            SkinWriter.Write(skin, path);
            Console.WriteLine($"Converted skin '{skin.Name}' to the current format.");
        }

        private static void ConvertPaletteContainer(string path)
        {
            var container = Deserialize<PaletteDynamicLoadContainer>(path);
            var dto = new PaletteContainerDto();
            foreach (var print in container.ColorPalettes)
                dto.ColorPalettes.Add(ToDto(print));

            File.WriteAllText(path, JsonSerializer.Serialize(dto, JsonOptions), Encoding.UTF8);
            Console.WriteLine($"Converted palette storage with {dto.ColorPalettes.Count} palette(s) to the current format.");
        }

        private static void ConvertSinglePalette(string path)
        {
            var print = Deserialize<ColorPalettePrint>(path);
            File.WriteAllText(path, JsonSerializer.Serialize(ToDto(print), JsonOptions), Encoding.UTF8);
            Console.WriteLine($"Converted palette '{print.Name}' to the current format.");
        }

        private static PaletteDto ToDto(ColorPalettePrint print)
        {
            return new PaletteDto
            {
                Name = print.Name,
                Purple = ToDto(print.Purple),
                Blue = ToDto(print.Blue),
                Green = ToDto(print.Green),
                Yellow = ToDto(print.Yellow),
                Red = ToDto(print.Red)
            };
        }

        private static SerializableColorDto ToDto(ColorPalettePrint.SerializableColor color)
        {
            return new SerializableColorDto
            {
                A = color.A,
                R = color.R,
                G = color.G,
                B = color.B,
                ScA = color.ScA,
                ScR = color.ScR,
                ScG = color.ScG,
                ScB = color.ScB
            };
        }

        private static T Deserialize<T>(string path) where T : class
        {
            var formatter = new BinaryFormatter { Binder = new LegacyBinder() };
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read))
            {
                return (T)formatter.Deserialize(stream);
            }
        }
    }
}
