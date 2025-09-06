namespace Tweaker.Core.Skins;

public class TexturePackData
{
    public List<NamedImage> RequiredParts { get; } = new();
    public List<NamedImage> OptionalParts { get; } = new();
    public List<NamedImage> Previews { get; } = new();
}
