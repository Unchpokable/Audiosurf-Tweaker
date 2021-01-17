using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Audiosurf_SkinChanger
{
    public partial class Form1 : Form
    {
        private string pathToSelectedSkin;
        public Form1()
        {
            InitializeComponent();
            openSkinDialog.Filter = "Audiosurf Skins (.askin)|*.askin";
            openSkinDialog.DefaultExt = ".askin";
        }

        private void button2_Click(object sender, EventArgs e)
        {

        }

        private void button3_Click(object sender, EventArgs e)
        {
            var dialogResult = openSkinDialog.ShowDialog();
            if (dialogResult == DialogResult.OK)
            {

            }
        }
    }
}
