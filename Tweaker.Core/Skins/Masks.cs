namespace Tweaker.Core.Skins;

using System.Text.RegularExpressions;

public static class Masks
{
    public static readonly string[] RequiredFiles = new[]
    {
        "cliff1-1.png", "cliff1-2.png", "cliff2-1.png", "cliff2-2.png",
        "hit1.png", "hit2.jpg", "particles1.png", "particles2.jpg", "particles3.jpg", "ring1A.png",
        "ring1B.png", "ring2A.jpg", "ring2B.jpg", "tilesflyup.png", "tiles.png"
    };

    public static readonly string[] OptionalFiles = new[]
    {
        "Skysphere_Black.png", "Skysphere_Grey.png", "Skysphere_White.png"
    };

    public static readonly Regex PreviewScreenshots = new Regex(@"^PreviewScreenshot.*\.(png|jpg|jpeg)$");
}

