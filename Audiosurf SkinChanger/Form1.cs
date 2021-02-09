using Audiosurf_SkinChanger.Engine;
using System;
using System.Windows.Forms;
using System.Configuration;
using System.Linq;
using System.IO;
using System.Drawing;
using Audiosurf_SkinChanger.Utilities;
using Microsoft.Win32;

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
        private PictureBox[] HitsImageGroup;
        private PictureBox[][] pictureBoxes;
        public string TempSkinName { get; set; }

        private Size stdSkysphereSize = new Size(180, 60);
        private Size stdTextureSize = new Size(64, 64);

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

            HitsImageGroup = new[]
            {
                hitPic1, hitPic2
            };

            pictureBoxes = new[] { SkySpherePreview, TilesTexturesImageGroup, ParticlesTexturesImageGroup, RingsTexturesImageGroup, HitsImageGroup };

            InternalWorker.SetUpDefaultSettings();
            InternalWorker.InitializeEnvironment();
            pathToGameTextbox.Text = EnvironmentalVeriables.gamePath;
            skinsFolderPathTextbox.Text = EnvironmentalVeriables.skinsFolderPath;
           
            skinPackager = new SkinPackager();
            LoadSkins("Skins");

            if (EnvironmentalVeriables.skinsFolderPath != "None")
                LoadSkins(EnvironmentalVeriables.skinsFolderPath);

            toolTip1.SetToolTip(cleanInstallCheck, "When installing in Clean Installation mode, the program will automatically delete all old Audiosurf textures, install the default skin and over it the one you choose.");
        }


        private void LoadSkins(string folder)
        {
            if (!Directory.Exists(folder))
            {
                MessageBox.Show("Can't load skins. Check skins folder", "Skins loading error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            foreach (var path in Directory.GetFiles(folder))
            {
                if (new FileInfo(path).Extension != ".askin")
                    continue;
                AudiosurfSkin skin = skinPackager.Decompile(path);
                EnvironmentalVeriables.Skins.Add(skin);
                SkinsListBox.Items.Add(skin);
            }

            if (SkinsListBox.Items.Count == 0)
                return;

            SkinsListBox.SelectedIndex = 0;
        }

        private void SavePathes(object sender, EventArgs e)
        {
            Configuration configuration = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);
            configuration.AppSettings.Settings["gamePath"].Value = pathToGameTextbox.Text;
            configuration.AppSettings.Settings["skinsPath"].Value = skinsFolderPathTextbox.Text;
            configuration.Save();

            ConfigurationManager.RefreshSection("appSettings");
            MessageBox.Show("Done!", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
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

                if (EnvironmentalVeriables.Skins.Select(x => x.Name).Contains(openedSkin.Name))
                {
                    MessageBox.Show("Skin already opened!", "Skin Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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

        private void ViewPathDialogShow(object sender, EventArgs e)
        {
            if (folderBrowserDialog1.ShowDialog() == DialogResult.OK)
            {
                var knownSender = sender as Button;

                if (Directory.Exists(folderBrowserDialog1.SelectedPath))
                {
                    if (knownSender.Name == "viewPathToGameBtn")
                        SetPathToGame(folderBrowserDialog1.SelectedPath);

                    else if (knownSender.Name == "viewPathToSkinsBtn")
                        SetPathToSkinsFolder(folderBrowserDialog1.SelectedPath);
                }
                SavePathes(null, null);
            }
        }

        private void SetPathToGame(string path)
        {
            EnvironmentalVeriables.gamePath = path;
            pathToGameTextbox.Text = path;
        }

        private void SetPathToSkinsFolder(string path)
        {
            EnvironmentalVeriables.skinsFolderPath = path;
            skinsFolderPathTextbox.Text = path;
            LoadSkins(path);
        }

        private void SkinsListBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            try
            {
                var selectedSkin = (AudiosurfSkin)SkinsListBox.SelectedItem;
                if (selectedSkin == null)
                    return;

                DrawPreviewOfSkin(selectedSkin);
                CurrentSkin = selectedSkin;
            }
            catch (Exception exc)
            {
                MessageBox.Show($"Ooops! May be you select empty skin or something else goes wrong! We cant load selected skin!\n Error message{exc.Message}", "Package Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void DrawPreviewOfSkin(AudiosurfSkin skin)
        {
            pictureBoxes.ForEach(x => x.ClearAll());
            tileFlyup.Image = ((Bitmap)skin.TilesFlyup).Rescale(stdTextureSize);
            FillPictureBoxGruopFromImageGroup(SkySpherePreview, skin.SkySpheres, stdSkysphereSize);
            FillPictureBoxGruopFromImageGroup(TilesTexturesImageGroup, SplitTilesSpritesheet((Bitmap)skin.Tiles), stdTextureSize);
            FillPictureBoxGruopFromImageGroup(ParticlesTexturesImageGroup, skin.Particles, stdTextureSize);
            FillPictureBoxGruopFromImageGroup(RingsTexturesImageGroup, skin.Rings, stdTextureSize);
            FillPictureBoxGruopFromImageGroup(HitsImageGroup, skin.Hits, stdTextureSize);
        }

        private void FillPictureBoxGruopFromImageGroup(PictureBox[] pictureBoxes, Bitmap[] images, Size newSize)
        {
            var imagesIterator = images.GetEnumerator();
            
            foreach (var picBox in pictureBoxes)
            {
                if (!imagesIterator.MoveNext())
                    return;
                picBox.Image = ((Bitmap)imagesIterator.Current).Rescale(newSize);
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
                    if (skin == null)
                    {
                        MessageBox.Show($"Error during packaging {folderBrowserDialog1.SelectedPath} into audiosurf skin. Please, Check selected folder", "Package Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        return;
                    }
                    if (MessageBox.Show("Do you want to name new skin?", "new skin", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                    {
                        new OpenNewSkinForm(this).ShowDialog(this);
                        skin.Name = TempSkinName;
                    }
                    else
                    {
                        string name = new DirectoryInfo(folderBrowserDialog1.SelectedPath).Name;
                        foreach (var skinName in EnvironmentalVeriables.Skins.Select(x => x.Name))
                        {
                            if (name == skinName)
                            {
                                MessageBox.Show("Skin with same name already exist! Enter new name for this skin", "Naming Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                new OpenNewSkinForm(this).ShowDialog(this);
                                name = TempSkinName;
                                break;
                            }
                        }
                        skin.Name = name;
                    }
                    EnvironmentalVeriables.Skins.Add(skin);
                    SkinsListBox.Items.Add(skin);
                    DrawPreviewOfSkin(skin);
                    CurrentSkin = skin;
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Ooops! May be you select empty skin or something else goes wrong! We cant load selected skin!\n Error message: {ex}", "Package Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void PackToSkinFile(object sender, EventArgs e)
        {
            if (CurrentSkin == null)
            {
                MessageBox.Show("Skin not selected!", "Export Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (skinsFolderPathTextbox.Text == "None")
            {
                skinPackager.CompileTo(CurrentSkin, "Skins");
            }
            else
                skinPackager.Compile(CurrentSkin);

            MessageBox.Show("Done!", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void InstallSkin(object sender, EventArgs e)
        { 
            AudiosurfSkin skin = (AudiosurfSkin)SkinsListBox.SelectedItem;
            if (skin == null)
            {
                MessageBox.Show("Can not install nothing. Please, select skin in list on left form side or add new skin and try again", "Installation Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (cleanInstallCheck.Checked)
            {
                Clean(EnvironmentalVeriables.gamePath);
                InstallSkin(skinPackager.Decompile(@"Skins\\default.askin"));
            }
            InstallSkin(skin);
            MessageBox.Show("Done!", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void Clean(string path)
        {
            DirectoryInfo di = new DirectoryInfo(path);

            foreach (FileInfo file in di.EnumerateFiles())
            {
                file.Delete();
            }
            foreach (DirectoryInfo dir in di.EnumerateDirectories())
            {
                dir.Delete(true);
            }
        }

        private void InstallSkin(AudiosurfSkin skin)
        {
            skin.SkySpheres.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
            skin.Hits.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
            skin.Tiles.Save(EnvironmentalVeriables.gamePath);
            skin.TilesFlyup.Save(EnvironmentalVeriables.gamePath);
            skin.Particles.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
            skin.Rings.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
            skin.Cliffs.Apply(x => x.Save(EnvironmentalVeriables.gamePath));
        }

        private void OpenSkinFromZip(object sender, EventArgs e)
        {
            MessageBox.Show("Not implemented now", "not avaiable", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }
}
