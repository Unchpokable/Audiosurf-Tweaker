namespace Tweaker.Core.Skins;

using System.Text.RegularExpressions;

public static class Masks
{
    public readonly static string[] RequiredFiles = new[]
    {
        "cliff1-1.png", "cliff1-2.png", "cliff2-1.png", "cliff2-2.png",
        "hit1.png", "hit2.jpg", "particles1.png", "particles2.jpg", "particles3.jpg", "ring1A.png",
        "ring1B.png", "ring2A.jpg", "ring2B.jpg", "tileslyup.png", "tiles.png"
    };

    public readonly static string[] OptionalFiles = new[]
    {
        "Skysphere_Black.png", "Skysphere_Grey.png", "Skysphere_White.png"
    };

    public readonly static Regex PeviewScreenshots = new Regex(@"^PreviewScreenshot.*\.(png|jpg|jpeg)$");
}

