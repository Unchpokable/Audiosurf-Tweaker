namespace Audiosurf_SkinChanger.Engine
{
    using System.IO.Compression;
    class SkinPackager
    {
        private const string skinExtension = @".askin";
        public bool Compile(AudiosurfSkin skin)
        {
            ZipFile.CreateFromDirectory(skin.Source, skin.Name + skinExtension);
            return false;
        }
    }
}
