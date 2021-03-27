using Audiosurf_SkinChanger.Engine;
using System;
using System.Windows.Forms;
using System.Configuration;
using System.Linq;
using System.IO;
using System.Drawing;
using System.Collections.Generic;
using FolderChecker;
using Audiosurf_SkinChanger.Utilities;

namespace Audiosurf_SkinChanger
{
    public partial class Form1 : Form
    {
        private SkinPackager skinPackager;
        private AudiosurfSkin currentSkin;
        private PictureBox[] skySpherePreview;
        private PictureBox[] tilesTexturesImageGroup;
        private PictureBox[] particlesTexturesImageGroup;
        private PictureBox[] ringsTexturesImageGroup;
        private PictureBox[] hitsImageGroup;
        private PictureBox[][] pictureBoxes;
        private CheckBox[] checkInstallCheckboxes;
        public string TempSkinName { get; set; }

        private Size stdSkysphereSize = new Size(180, 60);
        private Size stdTextureSize = new Size(64, 64);
        private Logger logger;

        private Dictionary<Button, Action<object, EventArgs>> packFolderIntoSkinButtonBehaviour;

        public Form1()
        {
            InitializeComponent();
            logger = new Logger();
            openSkinDialog.Filter = "Audiosurf Skins (.askin)|*.askin";
            openSkinDialog.DefaultExt = ".askin";
            SetSkinPartChecked();

            skySpherePreview = new[]
            {
                skySpherePic, SkyspherePic2, SkyspherePic3
            };

            tilesTexturesImageGroup = new[]
            {
                tilePic1, tilePic2, tilePic3, tilePic4
            };

            particlesTexturesImageGroup = new[]
            {
                partPic1, partPic2, partPic3
            };

            ringsTexturesImageGroup = new[]
            {
                ringPic1, ringPic2, ringPic3, ringPic4
            };

            hitsImageGroup = new[]
            {
                hitPic1, hitPic2
            };

            pictureBoxes = new[] { skySpherePreview, tilesTexturesImageGroup, particlesTexturesImageGroup, ringsTexturesImageGroup, hitsImageGroup };
            checkInstallCheckboxes = new[] { SkySpheresCheck, TilesCheck, RingsCheck, ParticlesCheck, HitsCheck};
            InternalWorker.SetUpDefaultSettings();
            InternalWorker.InitializeEnvironment();
            pathToGameTextbox.Text = EnvironmentalVeriables.gamePath;
            skinsFolderPathTextbox.Text = EnvironmentalVeriables.skinsFolderPath;
            skinPackager = new SkinPackager();
            GetCurrentlyInstalledSkin();
            LoadSkins();

            toolTip1.SetToolTip(cleanInstallCheck, "When installing in Clean Installation mode, the program will automatically delete all old Audiosurf textures, install the default skin and over it the one you choose.");
            packFolderIntoSkinButtonBehaviour = new Dictionary<Button, Action<object, EventArgs>>()
            {
                {button1, PackCurrentTextureFolderIntoSkin},
                {button3, PackAnyFolderIntoSkin}
            };
        }

        private void GetCurrentlyInstalledSkin()
        {
            if (FolderHashInfo.TryFind(EnvironmentalVeriables.gamePath, out FolderHashInfo state))
            {
                var actualState = FolderHashInfo.Create(EnvironmentalVeriables.gamePath);
                if (state.Equals(actualState))
                {
                    currentlyInstallLabel.Text = state.StateName;
                    return;
                }
                if (MessageBox.Show(
            @"Hey! Audiosurf Skin Changer has detected changes in your Audiosurf textures folder. 
It appears that you are using a texture set that is not a Skin Changer package, or this set is made up of several Skin Changer packages. 
Installing a skin may cause you to lose textures that are not part of the askin package. 
Do you want to keep your current texture set as an Audiosurf Skin Changer package?", "Warning",
                               MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes)
                {
                    currentlyInstallLabel.Text = "Custom";
                    PackCurrentTextureFolderIntoSkin(null, null);
                    FixCurrentState("Custom user skin");
                    return;
                }
                currentlyInstallLabel.Text = "Custom -Unsaved";
                return;
            }
            currentlyInstallLabel.Text = "Undetected";
        }

        private void FixCurrentState(string stateName)
        {
            EnvironmentChecker.SaveState(EnvironmentalVeriables.gamePath, stateName);
        }

        private void SetSkinPartChecked()
        {
            SkySpheresCheck.Checked = true;
            TilesCheck.Checked = true;
            RingsCheck.Checked = true;
            ParticlesCheck.Checked = true;
            HitsCheck.Checked = true;
        }

        private void LoadSkins()
        {
            LoadSkins("Skins", true);

            if (EnvironmentalVeriables.skinsFolderPath != "None")
                LoadSkins(EnvironmentalVeriables.skinsFolderPath);
        }


        private void LoadSkins(string folder, bool reload = false)
        {
            if (!Directory.Exists(folder))
            {
                MessageBox.Show("Can't load skins. Check skins folder", "Skins loading error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            if (reload)
            {
                EnvironmentalVeriables.Skins.Clear();
                SkinsListBox.Items.Clear();
            }
            foreach (var path in Directory.GetFiles(folder))
            {
                if (new FileInfo(path).Extension != ".askin")
                    continue;
                
                AudiosurfSkin skin = skinPackager.Decompile(path);
                if (skin == null)
                    return;

                var tempLink = new SkinLink(path, skin.Name);

                EnvironmentalVeriables.Skins.Add(tempLink);
                SkinsListBox.Items.Add(tempLink);
            }

            if (SkinsListBox.Items.Count == 0)
                return;

            SkinsListBox.SelectedIndex = 0;
            SkinsListBox.Invalidate();
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

                var tempLink = new SkinLink(openSkinDialog.FileName, openedSkin.Name);
                
                if (!SkinsListBox.Items.Contains(openedSkin))
                {
                    EnvironmentalVeriables.Skins.Add(tempLink);
                    SkinsListBox.Items.Add(tempLink);
                }
                SkinsListBox.SelectedItem = tempLink;
                currentSkin = openedSkin;
                DrawPreviewOfSkin(tempLink);
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
                var selectedSkin = (SkinLink)SkinsListBox.SelectedItem;
                if (selectedSkin == null)
                    return;

                DrawPreviewOfSkin(selectedSkin);
                currentSkin = selectedSkin.Load();
                SetSkinPartChecked();
            }
            catch (Exception exc)
            {
                MessageBox.Show($"Ooops! May be you select empty skin or something else goes wrong! We cant load selected skin!\n Error message{exc.Message}", "Package Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void DrawPreviewOfSkin(SkinLink link)
        {
            var skin = link.Load();
            pictureBoxes.ForEach(x => x.ClearAll());
            tileFlyup.Image = ((Bitmap)skin.TilesFlyup)?.Rescale(stdTextureSize);
            FillPictureBoxGruopFromImageGroup(skySpherePreview, skin.SkySpheres, stdSkysphereSize);
            FillPictureBoxGruopFromImageGroup(tilesTexturesImageGroup, ((Bitmap)skin.Tiles)?.Squarify(), stdTextureSize);
            FillPictureBoxGruopFromImageGroup(particlesTexturesImageGroup, skin.Particles, stdTextureSize);
            FillPictureBoxGruopFromImageGroup(ringsTexturesImageGroup, skin.Rings, stdTextureSize);
            FillPictureBoxGruopFromImageGroup(hitsImageGroup, skin.Hits, stdTextureSize);
        }

        private void FillPictureBoxGruopFromImageGroup(PictureBox[] pictureBoxes, Bitmap[] images, Size newSize)
        {
            if (pictureBoxes == null)
                return;
            if (images == null)
                return;
            if (newSize == null)
                throw new InvalidOperationException("HOW DO YOU SET STRUCT INTO NULL, RETARD?");

            var imagesIterator = images.GetEnumerator();

            foreach (var picBox in pictureBoxes)
            {
                if (!imagesIterator.MoveNext())
                    return;
                if (imagesIterator.Current == null)
                    continue;

                picBox.Image = ((Bitmap)imagesIterator.Current).Rescale(newSize);
            }
            return;
        }

        private void PackFolderIntoSkinRoute(object sender, EventArgs e)
        {
            var knownSender = sender as Button;
            if (knownSender == null)
                throw new NullReferenceException($"Sender is null");

            packFolderIntoSkinButtonBehaviour[knownSender]?.Invoke(sender, e);
        }

        private void PackAnyFolderIntoSkin(object sender, EventArgs e)
        {
            try
            {
                if (folderBrowserDialog1.ShowDialog() == DialogResult.OK)
                {
                    PackFolderIntoSkin(folderBrowserDialog1.SelectedPath);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Ooops! May be you select empty skin or something else goes wrong! We cant load selected skin!\n Error message: {ex}", "Package Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void PackCurrentTextureFolderIntoSkin(object sender, EventArgs e)
        {
            var path = EnvironmentalVeriables.gamePath;
            try
            {
                PackFolderIntoSkin(path);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Ooops! May be you select empty skin or something else goes wrong! We cant load selected skin!\n Error message: {ex}", "Package Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void PackFolderIntoSkin(string directory)
        {
            AudiosurfSkin skin = skinPackager.CreateSkinFromFolder(directory);
            if (skin == null)
            {
                MessageBox.Show($"Error during packaging {directory} into audiosurf skin. Please, Check selected folder", "Package Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            if (MessageBox.Show("Do you want to name new skin?", "new skin", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
            {
                new OpenNewSkinForm(this).ShowDialog(this);
                skin.Name = TempSkinName;
            }
            else
            {
                string name = new DirectoryInfo(directory).Name;
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
            skinPackager.CompileTo(skin, "Skins");
            var link = new SkinLink(@"Skins\" + skin.Name + SkinPackager.skinExtension, skin.Name);
            EnvironmentalVeriables.Skins.Add(link);
            SkinsListBox.Items.Add(link);
            DrawPreviewOfSkin(link);
            currentSkin = skin;
        }

        private void PackToSkinFile(object sender, EventArgs e)
        {
            if (currentSkin == null)
            {
                MessageBox.Show("Skin not selected!", "Export Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (skinsFolderPathTextbox.Text == "None")
            {
                ExportSkin(currentSkin, "Skins");
            }
            else
                ExportSkin(currentSkin);

            MessageBox.Show("Done!", "Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private bool ExportSkin(AudiosurfSkin skin, string path = null)
        {
            if (skin == null)
                return false;

            return path != null ? skinPackager.CompileTo(skin, path) : skinPackager.Compile(skin);
            
        }

        private void InstallSkin(object sender, EventArgs e)
        { 
            var linkToSelected = (SkinLink)SkinsListBox.SelectedItem;
            var skin = linkToSelected.Load();
            if (skin == null)
            {
                MessageBox.Show("Can not install nothing. Please, select skin in list on left form side or add new skin and try again", "Installation Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (cleanInstallCheck.Checked)
            {
                Clean(EnvironmentalVeriables.gamePath);
                InstallSkin(skinPackager.Decompile(@"Skins\default.askin"), forcedInstall: true);
            }
            InstallSkin(skin);
            EnvironmentChecker.SaveState(EnvironmentalVeriables.gamePath, skin.Name);
            GetCurrentlyInstalledSkin();
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

        private void InstallSkin(AudiosurfSkin skin, bool forcedInstall = false)
        {
            if (SkySpheresCheck.Checked || forcedInstall)
                skin.SkySpheres?.Apply(x => x?.Save(EnvironmentalVeriables.gamePath));
            if(HitsCheck.Checked || forcedInstall)
                skin.Hits?.Apply(x => x?.Save(EnvironmentalVeriables.gamePath));
            if (TilesCheck.Checked || forcedInstall)
            {
                skin.Tiles?.Save(EnvironmentalVeriables.gamePath);
                skin.TilesFlyup?.Save(EnvironmentalVeriables.gamePath);
            }
            if (ParticlesCheck.Checked || forcedInstall)
                skin.Particles?.Apply(x => x?.Save(EnvironmentalVeriables.gamePath));
            if (RingsCheck.Checked || forcedInstall)
                skin.Rings?.Apply(x => x?.Save(EnvironmentalVeriables.gamePath));
            skin.Cliffs?.Apply(x => x?.Save(EnvironmentalVeriables.gamePath));
        }

        private void OpenSkinEditor(object sender, EventArgs e)
        {
            var form = new Skin_Creator.SkinCreatorForm();
            form.OnSkinExprotrted += this.LoadSkins;
            form.Show();
        }

        private bool RemoveSelectedSkin(SkinLink skin)
        {
            try
            {
                File.Delete(skin.Path);
                SkinsListBox.Items.RemoveAt(SkinsListBox.SelectedIndex);
                SkinsListBox.SelectedIndex = 0;
                SkinsListBox.Invalidate();
                return true;
            }
            catch (Exception e)
            {
                MessageBox.Show("Can not remove selected skin!", "File error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                logger.Log("Error", e.ToString());
                return false;
            }
        }

        private void SkinsListBox_KeyDown(object sender, KeyEventArgs e)
        {
            if (MessageBox.Show("Are you sure to delete this skin?", "Remove skin", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                if (e.KeyCode == Keys.Delete)
                {
                    RemoveSelectedSkin((SkinLink)SkinsListBox.SelectedItem);
                }
        }
    }
}
