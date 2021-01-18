namespace Audiosurf_SkinChanger.Engine
{
    using Audiosurf_SkinChanger.Utilities;
    using System;
    using System.IO;
    using System.Runtime.Serialization;
    using System.Runtime.Serialization.Formatters.Binary;
    using System.Windows.Forms;

    class SkinPackager
    {
        private const string skinExtension = @".askin";
        private Logger logger;

        public SkinPackager()
        {
            logger = new Logger();
        }

        public bool Compile(AudiosurfSkin skin)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                Stream filestream = new FileStream(EnvironmentalVeriables.OutputPath + skin.Name + skinExtension, FileMode.Create, FileAccess.Write, FileShare.None);
                formatter.Serialize(filestream, skin);
                filestream.Close();
                return true;
            }
            catch (Exception e)
            {
                MessageBox.Show($"Ooops! Something goes wrong! We cant save your skin {skin.Name}!\n Exception message: {e.Message}.\n Stack trace written in log file");
                logger.Log("ERROR",e.Message + "\n" + e.StackTrace);
                return false;
            }
        }

        public AudiosurfSkin Decompile(string path)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                Stream skinFileStream = new FileStream(path, FileMode.Open);
                return (AudiosurfSkin)formatter.Deserialize(skinFileStream);
            }
            catch(Exception e)
            {
                MessageBox.Show($"Ooops! Something goes wrong! We cant load your skin {path}!\n Exception message: {e.Message}.\n Stack trace written in log file");
                logger.Log("ERROR", e.Message + "\n" + e.StackTrace);
                return null;
            }
        }
    }
}
