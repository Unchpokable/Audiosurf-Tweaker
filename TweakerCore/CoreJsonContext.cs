using System.Text.Json;
using System.Text.Json.Serialization;
using TweakerCore.Engine;
using TweakerCore.FolderChecker;
using TweakerCore.PluginInstall;

namespace TweakerCore
{
    [JsonSourceGenerationOptions(WriteIndented = true)]
    [JsonSerializable(typeof(SkinManifest))]
    [JsonSerializable(typeof(FolderHashInfo))]
    [JsonSerializable(typeof(PluginDeployManifest))]
    internal partial class CoreJsonContext : JsonSerializerContext
    {
        internal static CoreJsonContext Compact { get; } = new(new JsonSerializerOptions());
    }
}
