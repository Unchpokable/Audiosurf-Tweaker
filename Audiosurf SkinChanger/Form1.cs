using Audiosurf_SkinChanger.Engine;
using System;
using System.Windows.Forms;
using System.Configuration;
using System.Collections.Specialized;
using System.IO;

namespace Audiosurf_SkinChanger
{
    public partial class Form1 : Form
    {
        private SkinPackager skinPackager;
        private PictureBox SkySpherePreview;
        private PictureBox[] TilesTexturesImageGroup;
        private PictureBox[] ParticlesTexturesImageGroup;
        private PictureBox[] RingsTexturesImageGroup;

        public Form1()
        {
            InitializeComponent();
            openSkinDialog.Filter = "Audiosurf Skins (.askin)|*.askin";
            openSkinDialog.DefaultExt = ".askin";

            SkySpherePreview = skySpherePic;
            TilesTexturesImageGroup = new[]
            {
                tilePic1, tilePic2, tilePic3, tilePic4, tilePic5
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
    }
}
