using Audiosurf_SkinChanger.Engine;
using System;
using System.Windows.Forms;
using System.Configuration;
using System.Linq;
using System.IO;
using System.Drawing;
using Audiosurf_SkinChanger.Utilities;

namespace Audiosurf_SkinChanger
{
    public partial class Form1 : Form
    {
        private SkinPackager skinPackager;
        private AudiosurfSkin CurrentSkin;
        private PictureBox[] SkySpherePreview;
        private PictureBox[] TilesTexturesImageGroup;
        private PictureBox[] ParticlesTexturesImageGroup;
        private PictureBox[] RingsTexturesImageGroup;
        private PictureBox[][] pictureBoxes;

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

            pictureBoxes = new[] { SkySpherePreview, TilesTexturesImageGroup, ParticlesTexturesImageGroup, RingsTexturesImageGroup };

            pathToGameTextbox.Text = ConfigurationManager.AppSettings.Get("gamePath");
            EnvironmentalVeriables.gamePath = ConfigurationManager.AppSettings.Get("gamePath");
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

                if (!SkinsListBox.Items.Contains(openedSkin))
                {
                    EnvironmentalVeriables.Skins.Add(openedSkin);
                    SkinsListBox.Items.Add(openedSkin);
                }
                SkinsListBox.SelectedItem = openedSkin;
                CurrentSkin = openedSkin;
                DrawPreviewOfSkin(openedSkin);
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
                var selectedSkin = (AudiosurfSkin)SkinsListBox.SelectedItem;
                DrawPreviewOfSkin(selectedSkin);
                CurrentSkin = selectedSkin;
            }
            catch (Exception exc)
            {
                MessageBox.Show($"Ooops! May be you select empty skin or something else goes wrong! We cant load selected skin!\n Error message{exc.Message}");
            }
        }

        private void DrawPreviewOfSkin(AudiosurfSkin skin)
        {
            pictureBoxes.ForEach(x => x.ClearAll());
            tileFlyup.Image = ((Bitmap)skin.TilesFlyup).Rescale(64,64);
            FillPictureBoxGruopFromImageGroup(SkySpherePreview, skin.SkySpheres);
            FillPictureBoxGruopFromImageGroup(TilesTexturesImageGroup, SplitTilesSpritesheet((Bitmap)skin.Tiles));
            FillPictureBoxGruopFromImageGroup(ParticlesTexturesImageGroup, skin.Particles);
            FillPictureBoxGruopFromImageGroup(RingsTexturesImageGroup, skin.Rings);
        }

        private void FillPictureBoxGruopFromImageGroup(PictureBox[] pictureBoxes, Bitmap[] images)
        {
            var imagesIterator = images.GetEnumerator();
            
            foreach (var picBox in pictureBoxes)
            {
                if (!imagesIterator.MoveNext())
                    return;
                picBox.Image = ((Bitmap)imagesIterator.Current).Rescale(64,64);
            }
            return;
        }

        private Bitmap[] SplitTilesSpritesheet(Bitmap spritesheet)
        {
            int widthThird = (int)((double)spritesheet.Width / 2.0 + 0.5);
            int heightThird = (int)((double)spritesheet.Height / 2.0 + 0.5);
            Bitmap[,] bmps = new Bitmap[2, 2];
            for (int i = 0; i < 2; i++)
                for (int j = 0; j < 2; j++)
                {
                    bmps[i, j] = new Bitmap(widthThird, heightThird);
                    Graphics g = Graphics.FromImage(bmps[i, j]);
                    g.DrawImage(spritesheet, new Rectangle(0, 0, widthThird, heightThird), new Rectangle(j * widthThird, i * heightThird, widthThird, heightThird), GraphicsUnit.Pixel);
                    g.Dispose();
                }
            return bmps.Cast<Bitmap>().ToArray();
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
                    CurrentSkin = skin;
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Ooops! May be you select empty skin or something else goes wrong! We cant load selected skin!\n Error message{ex.Message}");
            }
        }

        private void PackToSkinFile(object sender, EventArgs e)
        {
            skinPackager.Compile(CurrentSkin);
        }

        private void InstallSkin(object sender, EventArgs e)
        {
            AudiosurfSkin skin = (AudiosurfSkin)SkinsListBox.SelectedItem;
            skin.SkySpheres.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
            skin.Hits.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
            skin.Tiles.Save(EnvironmentalVeriables.gamePath);
            skin.TilesFlyup.Save(EnvironmentalVeriables.gamePath);
            skin.Particles.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
            skin.Rings.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
            skin.Cliffs.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
        }
    }
}
