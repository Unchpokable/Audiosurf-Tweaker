using Audiosurf_SkinChanger.Engine;
using System;
using System.Windows.Forms;
using System.Configuration;
using System.Collections.Specialized;
using System.IO;
using System.Drawing;
using Audiosurf_SkinChanger.Utilities;

namespace Audiosurf_SkinChanger
{
    public partial class Form1 : Form
    {
        private SkinPackager skinPackager;
        private PictureBox[] SkySpherePreview;
        private PictureBox[] TilesTexturesImageGroup;
        private PictureBox[] ParticlesTexturesImageGroup;
        private PictureBox[] RingsTexturesImageGroup;

        public Form1()
        {
            InitializeComponent();
            openSkinDialog.Filter = "Audiosurf Skins (.askin)|*.askin";
            openSkinDialog.DefaultExt = ".askin";

            SkySpherePreview = new[]
            {
                skySpherePic, SkyspherePic2, SkyspherePic3
            };

            TilesTexturesImageGroup = new[]
            {
                tilePic1, tilePic2, tilePic3, tilePic4
            };

            ParticlesTexturesImageGroup = new[]
            {
                partPic1, partPic2, partPic3
            };

            RingsTexturesImageGroup = new[]
            {
                ringPic1, ringPic2, ringPic3, ringPic4
            };

            pathToGameTextbox.Text = ConfigurationManager.AppSettings.Get("gamePath");
            skinPackager = new SkinPackager();
        }

        private void SavePath(object sender, EventArgs e)
        {
            Configuration configuration = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);
            configuration.AppSettings.Settings["gamePath"].Value = pathToGameTextbox.Text;
            configuration.Save();

            ConfigurationManager.RefreshSection("appSettings");
        }

        private void OpenSkinBtnClick(object sender, EventArgs e)
        {
            if (openSkinDialog.ShowDialog() == DialogResult.OK)
            {
                AudiosurfSkin openedSkin = skinPackager.Decompile(openSkinDialog.FileName);
                if (openedSkin == null)
                {
                    MessageBox.Show("Cant open empty .askin file", "Empty Skin!", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                EnvironmentalVeriables.Skins.Add(openedSkin);
                SkinsListBox.Items.Add(openedSkin);
                SkinsListBox.SelectedItem = openedSkin;
            }
        }

        private void viewPathToGameBtn_Click(object sender, EventArgs e)
        {
            if (folderBrowserDialog1.ShowDialog() == DialogResult.OK)
            {
                if (Directory.Exists(folderBrowserDialog1.SelectedPath))
                {
                    EnvironmentalVeriables.gamePath = folderBrowserDialog1.SelectedPath;
                    pathToGameTextbox.Text = folderBrowserDialog1.SelectedPath;
                }
            }
        }

        private void SkinsListBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            try
            {
                DrawPreviewOfSkin((AudiosurfSkin)SkinsListBox.Items[SkinsListBox.SelectedIndex]);
            }
            catch (Exception exc)
            {
                MessageBox.Show($"Ooops! May be you select empty skin or something else goes wrong! We cant load selected skin!\n Error message{exc.Message}");
            }
        }

        private void DrawPreviewOfSkin(AudiosurfSkin skin)
        {
            tileFlyup.Image = skin.TilesFlyup;
            FillPictureBoxGruopFromImageGroup(SkySpherePreview, skin.SkySpheres);
            FillPictureBoxGruopFromImageGroup(TilesTexturesImageGroup, SplitTilesSpritesheet(skin.Tiles));
            FillPictureBoxGruopFromImageGroup(ParticlesTexturesImageGroup, skin.Particles);
            FillPictureBoxGruopFromImageGroup(RingsTexturesImageGroup, skin.Rings);
        }

        private void FillPictureBoxGruopFromImageGroup(PictureBox[] pictureBoxes, ImageGroup images)
        {
            using (var imagesIterator = images.Group.GetEnumerator())
            {
                foreach (var picBox in pictureBoxes)
                {
                    if (!imagesIterator.MoveNext())
                        return;
                    picBox.Image = imagesIterator.Current;
                }
            }
            return;
        }

        private ImageGroup SplitTilesSpritesheet(Bitmap spritesheet)
        {
            var group = new ImageGroup("tiles");

            for (int i = 0; i < 256; i+= 128)
            for (int k = 0; k < 256; k+= 128)
            {
                var bitmap = new Bitmap(128, 128);
                using (var g = Graphics.FromImage(bitmap))
                {
                    g.DrawImage(spritesheet, i,k, 128, 128);
                }
                group.AddImage((Bitmap)bitmap.Clone());
             }
            return group;
        }

        private void PackFolderIntoSkin(object sender, EventArgs e)
        {
            try
            {
                if (folderBrowserDialog1.ShowDialog() == DialogResult.OK)
                {
                    AudiosurfSkin skin = skinPackager.CreateSkinFromFolder(folderBrowserDialog1.SelectedPath);
                    skin.Name = "new skin";
                    EnvironmentalVeriables.Skins.Add(skin);
                    SkinsListBox.Items.Add(skin);
                    DrawPreviewOfSkin(skin);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Ooops! May be you select empty skin or something else goes wrong! We cant load selected skin!\n Error message{ex.Message}");
            }
        }
    }
}
