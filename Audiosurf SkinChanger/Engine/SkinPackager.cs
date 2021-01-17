namespace Audiosurf_SkinChanger.Engine
{
    using System.IO;
    using System.Runtime.Serialization;
    using System.Runtime.Serialization.Formatters.Binary;
    class SkinPackager
    {
        private const string skinExtension = @".askin";
        public bool Compile(AudiosurfSkin skin)
        {
            IFormatter formatter = new BinaryFormatter();
            Stream filestream = new FileStream()

        }
    }
}
