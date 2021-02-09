using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Audiosurf_SkinChanger.Skin_Creator
{
    public partial class SkinCreatorForm : Form
    {

        private Dictionary<string, PictureBox> AssotiateTable;
        private Dictionary<string, Bitmap> States;

        public SkinCreatorForm()
        {
            InitializeComponent();
            LoadIdleImages();
            CreateAssotiativeTable();
            CreateStateAssotiatieTable();
        }

        private void CreateAssotiativeTable()
        {
            AssotiateTable = new Dictionary<string, PictureBox>()
            {
                { "Sphere1", Sphere1 },
                { "Sphere2", Sphere2 },
                { "Sphere3", Sphere3 }
            };
        }

        private void CreateStateAssotiatieTable()
        {
            States = new Dictionary<string, Bitmap>
            {
                {"SphereIdle", Images.Add_Skysphere },
                {"SphereActive", Images.Add_SkySphere_Active }
            };
        }

        private void LoadIdleImages()
        {
            Sphere1.Image = Images.Add_Skysphere;
            Sphere2.Image = Images.Add_Skysphere;
            Sphere3.Image = Images.Add_Skysphere;
        }

        private void splitContainer1_Panel2_Paint(object sender, PaintEventArgs e)
        {

        }

        private void SetActiveSphere(object sender, EventArgs e)
        {
            var knownSender = sender as PictureBox;
            AssotiateTable[knownSender.Name].Image = States["SphereActive"];
        }

        private void Sphere1_MouseLeave(object sender, EventArgs e)
        {
            var knownSender = sender as PictureBox;
            AssotiateTable[knownSender.Name].Image = States["SphereIdle"];
        }
    }
}
