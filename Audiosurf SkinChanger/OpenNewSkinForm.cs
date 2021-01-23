using System;
using System.Windows.Forms;
using System.IO;
using System.Linq;

namespace Audiosurf_SkinChanger
{
    public partial class OpenNewSkinForm : Form
    {
        private Form1 parent;

        public OpenNewSkinForm(Form p)
        {
            InitializeComponent();
            parent = p as Form1;
        }

        private void button1_Click(object sender, EventArgs e)
        {
            if (string.IsNullOrEmpty(textBox1.Text) || string.IsNullOrWhiteSpace(textBox1.Text))
            {
                MessageBox.Show("Please, enter new skin name", "Naming error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            foreach(var skinName in EnvironmentalVeriables.Skins.Select(x => x.Name))
            {
                if (skinName == textBox1.Text)
                {
                    MessageBox.Show("Name already used. Please, enter another skin name", "Naming error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    textBox1.Text = "";
                    textBox1.Invalidate();
                    return;
                }
            }

            parent.TempSkinName = textBox1.Text;
            this.Close();
        }
    }
}
