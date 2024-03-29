﻿using ChangerAPI.Utilities;
using System;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Formatters.Binary;

namespace ChangerAPI.Engine
{
    using Env = EnvironmentalVeriables;

    public static class SkinPackager
    {
        public static readonly string SkinExtension = @".tasp";
        public static string OutputPath { get; set; }

        private static string[] _texturesNames;

        private static readonly string _defaultOutput = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);

        static SkinPackager()
        {
            _texturesNames = new[]
            {
                "cliff-1.png", "cliff-2.png", "cliff2-1.png", "cliff2-1.png", "hit1.png", "hit2.png",
                "particles1.png", "particles2.png", "particles3.png", "ring1A.png", "ring1B.png", "ring2A.jpg", "ring2B",
                "Skyshpere_Black.png", "Skysphere_Grey.png", "Skyshphere_White.png",
                "tileflyup.png", "tiles.png"
            };
        }


        public static bool Compile(AudiosurfSkinExtended skin)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                using (Stream filestream = new FileStream((OutputPath ?? _defaultOutput) + @"\\" + skin.Name + SkinExtension, FileMode.OpenOrCreate))
                {
                    formatter.Serialize(filestream, skin);
                }
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        public static bool CompileToPath(AudiosurfSkinExtended skin, string path)
        {
            return CompileToFile(skin, path + @"\\" + skin.Name + SkinExtension);
        }

        public static bool CompileToFile(AudiosurfSkinExtended skin, string file)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                using (Stream filestream = new FileStream(file, FileMode.OpenOrCreate))
                {
                    formatter.Serialize(filestream, skin);
                }
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        public static bool RewriteCompile(AudiosurfSkinExtended skin, string path)
        {
            try
            {
                IFormatter formatter = new BinaryFormatter();
                using (Stream filestream = new FileStream(path, FileMode.Create))
                {
                    formatter.Serialize(filestream, skin);
                }
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        public static AudiosurfSkinExtended Decompile(string path)
        {
            if (!File.Exists(path))
                return null;

            if (new[] { Env.LegacySkinExtention, Env.ActualSkinExtention }.All(ext => ext != Path.GetExtension(path)))
                return null;

            try
            {
                AudiosurfSkinExtended result = null;
                IFormatter formatter = new BinaryFormatter();
                using (Stream skinFileStream = new FileStream(path, FileMode.Open))
                {
                    result = (AudiosurfSkinExtended)formatter.Deserialize(skinFileStream);
                    result.Source = path;
                }
                return result.Clone();
            }

            catch (IOException)
            {
                return null;
            }

            catch (Exception)
            {
                return null;
            }
        }


        public static AudiosurfSkinExtended CreateSkinFromFolder(string path)
        {
            var result = new AudiosurfSkinExtended();
            
            string[] allPictures = Directory.GetFiles(path);
            if (!allPictures.Any(fileName => _texturesNames.Contains(Path.GetFileName(fileName))))
                return null;

            result.Cliffs = GetAllImagesByNameMask("cliffs", Env.CliffImagesMask, path);
            result.Hits = GetAllImagesByNameMask("hits", Env.HitImageMask, path);
            result.Particles = GetAllImagesByNameMask("particles", Env.ParticlesImageMask, path);
            result.Rings = GetAllImagesByNameMask("rings", Env.RingsImageMask, path);
            result.SkySpheres = GetAllImagesByNameMask("skysphere", Env.SkysphereImagesMask, path);

            ImageGroup tiles = GetAllImagesByNameMask("tiles", "tiles.png", path);
            ImageGroup tileflyup = GetAllImagesByNameMask("tiles flyup", "tileflyup.png", path);
            if (Directory.Exists(path + @"\Screenshots"))
            {
                result.Previews = GetAllImagesByNameMask("Screenshots", "screenshot", path + @"\Screenshots");
                result.Cover = result.Previews?.Group?.FirstOrDefault();
            }

            if (tiles.Group.Count > 1 || tileflyup.Group.Count > 1)
            {
                return null;
            }

            result.Tiles = new NamedBitmap("tiles.png", (Bitmap)tiles);
            result.TilesFlyup = new NamedBitmap("tileflyup.png", (Bitmap)tileflyup);
            return result;
        }

        private static ImageGroup GetAllImagesByNameMask(string groupName, string nameMask, string path)
        {
            var group = new ImageGroup(groupName);

            string[] allFiles = Directory.GetFiles(path);

            foreach(var file in allFiles)
            {
                var origName = Path.GetFileName(file);
                var fname = origName.ToLower();
                var fileExt = Path.GetExtension(fname);
                if (fname.Contains(nameMask) && new[] { ".png", ".jpg" }.Any(x => x == fileExt))
                {
                    var image = new NamedBitmap(origName, Image.FromFile(file));
                    group.AddImage(image);
                }
            }

            return group;
        }
    }
}
