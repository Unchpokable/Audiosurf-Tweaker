using System.Text.Json.Serialization;

namespace TweakerUI.Models
{
    [JsonSourceGenerationOptions(WriteIndented = true)]
    [JsonSerializable(typeof(ColorPalettePrint))]
    [JsonSerializable(typeof(PaletteDynamicLoadContainer))]
    internal partial class PaletteJsonContext : JsonSerializerContext
    {
    }
}
