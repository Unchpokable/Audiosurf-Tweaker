namespace ChangerAPI.Utilities
{
    using ChangerAPI.Engine;

    class SkinLink
    {
        public string Path;
        public string Name;

        private SkinPackager packager; 
         
        public SkinLink(string path, string skinName)
        {
            Path = path;
            Name = skinName;
            packager = new SkinPackager();
        }

        public AudiosurfSkin Load()
        {
            return packager.Decompile(Path);
        }

        public override string ToString()
        {
            return Name;
        }
    }
}
