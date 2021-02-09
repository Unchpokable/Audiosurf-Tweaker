namespace Audiosurf_SkinChanger.Engine
{
    using Audiosurf_SkinChanger.Utilities;
    using System;
    using System.Collections.Generic;
    using System.Drawing;
    using System.IO;
    using System.Linq;
    using System.Runtime.Serialization;
    using System.Runtime.Serialization.Formatters.Binary;
    using System.Windows.Forms;

    class SkinPackager
    {
        private const string skinExtension = @".askin";
        private Logger logger;
        private string[] texturesNames;
        private IList<ImageGroup> textureGroups;
        private string[] Masks;
        private readonly string defaultOutput = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);

        public SkinPackager()
        {
            logger = new Logger();
            texturesNames = new[]
            {
                "cliff-1.png", "cliff-2.png", "cliff2-1.png", "cliff2-1.png", "hit1.png", "hit2.png",
                "particles1.png", "particles2.png", "particles3.png", "ring1A.png", "ring1B.png", "ring2A.jpg", "ring2B",
                "Skyshpere_Black.png", "Skysphere_Grey.png", "Skyshphere_White.png",
                "tileflyup.png", "tiles.png"
            };

            Masks = new[]
            {
                EnvironmentalVeriables.CliffImagesMask,
                EnvironmentalVeriables.HitImageMask,
                EnvironmentalVeriables.ParticlesImageMask,
                EnvironmentalVeriables.RingsImageMask,
                EnvironmentalVeriables.SkysphereImagesMask,
            };
        }

        public bool Compile(AudiosurfSkin skin)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                using (Stream filestream = new FileStream((EnvironmentalVeriables.skinsFolderPath ?? defaultOutput) + @"\\" + skin.Name + skinExtension, FileMode.OpenOrCreate))
                {
                    formatter.Serialize(filestream, skin);
                }
                return true;
            }
            catch (Exception e)
            {
                MessageBox.Show($"Ooops! Something goes wrong! We cant save your skin {skin.Name}!\n Exception message: {e.Message}.\n Stack trace written in log file",
                                "Error!", MessageBoxButtons.OK, MessageBoxIcon.Error);
                logger.Log("ERROR", e.ToString());
                return false;
            }
        }

        public bool CompileTo(AudiosurfSkin skin, string path)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                using (Stream filestream = new FileStream(path + @"\\" + skin.Name + skinExtension, FileMode.OpenOrCreate))
                {
                    formatter.Serialize(filestream, skin);
                }
                return true;
            }
            catch (Exception e)
            {
                MessageBox.Show($"Ooops! Something goes wrong! We cant save your skin {skin.Name}!\n Exception message: {e.Message}.\n Stack trace written in log file",
                                "Error!", MessageBoxButtons.OK, MessageBoxIcon.Error);
                logger.Log("ERROR", e.ToString());
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
                MessageBox.Show($"Ooops! Something goes wrong! We cant load your skin {path}!\n Exception message: {e.Message}.\n Stack trace written in log file",
                                "Error!", MessageBoxButtons.OK, MessageBoxIcon.Error);
                logger.Log("ERROR", e.ToString());
                return null;
            }
        }

        public AudiosurfSkin CreateSkinFromFolder(string path)
        {
            var result = new AudiosurfSkin();
            
            string[] AllPictures = Directory.GetFiles(path);
            if (!AllPictures.Any(fileName => texturesNames.Contains(Path.GetFileName(fileName))))
                return null;

            foreach(var mask in Masks)
            {
                if (mask == EnvironmentalVeriables.CliffImagesMask)
                    result.Cliffs = GetAllImagesByNameMask("cliffs", mask, path);
                if (mask == EnvironmentalVeriables.HitImageMask)
                    result.Hits = GetAllImagesByNameMask("hits", mask, path);
                if (mask == EnvironmentalVeriables.ParticlesImageMask)
                    result.Particles = GetAllImagesByNameMask("particles", mask, path);
                if (mask == EnvironmentalVeriables.RingsImageMask)
                    result.Rings = GetAllImagesByNameMask("rings", mask, path);
                if (mask == EnvironmentalVeriables.SkysphereImagesMask)
                    result.SkySpheres = GetAllImagesByNameMask("skysphere", mask, path);
            }

            ImageGroup tiles = GetAllImagesByNameMask("tiles", "tiles.png", path);
            ImageGroup tileflyup = GetAllImagesByNameMask("tiles flyup", "tileflyup.png", path);

            if (tiles.Group.Count > 1 || tileflyup.Group.Count > 1)
            {
                MessageBox.Show("Ooops! May be folder with your skin contain more that 1 texture with name 'tiles.png' or 'tileflyup.png' (or ***tiles.png / ***tileflyup.png).\nPlease, check folder with skin and correct names of textures", "Package Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return null;
            }

            result.Tiles = new NamedBitmap("tiles.png", (Bitmap)tiles);
            result.TilesFlyup = new NamedBitmap("tileflyup.png", (Bitmap)tileflyup);
            return result;
        }

        private ImageGroup GetAllImagesByNameMask(string groupName, string nameMask, string path)
        {
            var group = new ImageGroup(groupName);

            string[] AllFiles = Directory.GetFiles(path);

            foreach(var file in AllFiles)
            {
                var origName = Path.GetFileName(file);
                var fname = origName.ToLower();
                if (fname.Contains(nameMask))
                {
                    var image = new NamedBitmap(origName, Image.FromFile(file));
                    group.AddImage(image);
                }
            }

            return group;
        }
    }
}
