using System.Text.Json.Serialization;

namespace QuickPlayerCore
{
    [JsonSourceGenerationOptions(WriteIndented = true)]
    [JsonSerializable(typeof(Playlist))]
    internal partial class PlayerJsonContext : JsonSerializerContext
    {
    }
}
