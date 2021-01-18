using Audiosurf_SkinChanger.Engine;
using System;
using System.Windows.Forms;

namespace Audiosurf_SkinChanger
{
    public partial class Form1 : Form
    {
        private SkinPackager skinPackager;

        public Form1()
        {
            InitializeComponent();
            openSkinDialog.Filter = "Audiosurf Skins (.askin)|*.askin";
            openSkinDialog.DefaultExt = ".askin";
            skinPackager = new SkinPackager();
        }

        private void button2_Click(object sender, EventArgs e)
        {

        }

        private void openSkinBtn_Click(object sender, EventArgs e)
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
    }
}
