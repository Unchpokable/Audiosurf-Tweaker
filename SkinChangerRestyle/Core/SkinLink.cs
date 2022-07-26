namespace SkinChangerRestyle.Core
{
    using ChangerAPI.Engine;
    using SkinChangerRestyle.Core.Extensions;
    using System.Drawing;
    using System.Windows.Media;

    public class SkinLink
    {
        public string Path { get; set; }
        public string Name { get; set; }
        public ImageSource Cover { get; private set; } 

        public SkinLink(string path, AudiosurfSkinExtended source)
        {
            Path = path;
            Name = $"{source.Name}";
            Cover = new Bitmap((Bitmap)source.Cover).Rescale(.5f, .5f).ToImageSource();
        }

        public AudiosurfSkinExtended Load()
        {
            var skin = SkinPackager.Decompile(Path);
            if (skin.GetType() == typeof(AudiosurfSkinExtended)) return skin;
            return AudiosurfSkinExtended.Reinterpret(skin);
        }

        public override string ToString()
        {
            return Name;
        }
    }
}
