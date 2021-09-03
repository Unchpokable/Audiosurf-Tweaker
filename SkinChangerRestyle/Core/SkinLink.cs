namespace SkinChangerRestyle.Core
{
    using ChangerAPI.Engine;

    class SkinLink
    {
        public string Path;
        public string Name;
         
        public SkinLink(string path, string skinName)
        {
            Path = path;
            Name = skinName;
        }

        public AudiosurfSkinExtended Load()
        {
            return AudiosurfSkinExtended.Reinterpret(SkinPackager.Decompile(Path));
        }

        public override string ToString()
        {
            return Name;
        }
    }
}
